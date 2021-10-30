#include "UnityPrefix.h"
#include "ImportMesh.h"
#include "Runtime/Utilities/PathNameUtility.h"

// UNFINISHED CODE, patience :)


// Remotely derived from glm.c
// Nate Robins, 1997, 2000
// nate@pobox.com, http://www.pobox.com/~nate
// Wavefront OBJ model file format reader/writer/manipulator.

// defines a triangle in a model.
struct GLMtriangle {
	UInt32 vindices[3]; // vertices
	UInt32 nindices[3]; // normals
	UInt32 tindices[3]; // UVs
};

// group in a model
struct GLMgroup {
	std::string name;
	std::vector<UInt32> triangles;
	int				  triindex;
	UInt32            material;
	GLMgroup* next;
};

struct ImportOBJState {
	std::string pathname;	// path to file
	std::string mtllibname;	// path of material library
	ImportScene*	m_Scene;
	
	UInt32   numvertices;         /* number of vertices in model */
	float* vertices;            /* array of vertices  */
	
	UInt32   numnormals;          /* number of normals in model */
	float* normals;             /* array of normals */
	
	UInt32   numtexcoords;        /* number of texcoords in model */
	float* texcoords;           /* array of texture coordinates */
	
	UInt32       numtriangles;    /* number of triangles in model */
	GLMtriangle* triangles;       /* array of triangles */
	
	UInt32       numgroups;       /* number of groups in model */
	GLMgroup*    groups;          /* linked list of groups */
	
	float position[3];          /* position of the model */	
};


//void glmReverseWinding(ImportOBJState* model);



#define T(x) (model->triangles[(x)])


/* glmFindGroup: Find a group in the model */
GLMgroup* glmFindGroup(ImportOBJState* model, const char* name)
{
    GLMgroup* group;
    
    AssertIf( !model );
    
    group = model->groups;
    while(group) {
        if (!strcmp(name, group->name.c_str()))
            break;
        group = group->next;
    }
    
    return group;
}

/* glmAddGroup: Add a group to the model */
GLMgroup*
glmAddGroup(ImportOBJState* model, char* name)
{
    GLMgroup* group;
    
    group = glmFindGroup(model, name);
    if (!group) {
		group = new GLMgroup();
		group->triindex = 0;
        group->name = name;
        group->material = 0;
        group->next = model->groups;
        model->groups = group;
        model->numgroups++;
    }
    
    return group;
}

UInt32
glmFindMaterial(ImportOBJState* model, char* name)
{
    for (size_t i = 0; i < model->m_Scene->materials.size(); i++) {
        if( !strcmp(model->m_Scene->materials[i].name.c_str(), name) )
			return i;
    }
    
    // didn't find the name, so print a warning and return the default  material (0)
    printf_console("glmFindMaterial():  can't find material \"%s\".\n", name);
	return 0;
}



static bool glmReadMTL( ImportOBJState* model, const char* name )
{
    char    buf[128];
    UInt32 nummaterials;
    
	std::string dirname = DeleteLastPathNameComponent(model->pathname);
	std::string filename = AppendPathName( dirname, name );
    
    FILE* file = fopen(filename.c_str(), "rt");
    if (!file) {
        fprintf(stderr, "glmReadMTL() failed: can't open material file \"%s\".\n", filename.c_str());
		return false;
    }
    
    /* count the number of materials in the file */
    nummaterials = 1;
    while(fscanf(file, "%s", buf) != EOF) {
        switch(buf[0]) {
			case '#':               /* comment */
				/* eat up rest of line */
				fgets(buf, sizeof(buf), file);
				break;
			case 'n':               /* newmtl */
				fgets(buf, sizeof(buf), file);
				nummaterials++;
				sscanf(buf, "%s %s", buf, buf);
				break;
			default:
				/* eat up rest of line */
				fgets(buf, sizeof(buf), file);
				break;
        }
    }
    
    rewind(file);

	std::vector<ImportMaterial>& materials = model->m_Scene->materials;
    materials.clear();
    materials.resize( nummaterials );
    
    materials[0].name = "default";
	
	float dummy;
    
    /* now, read in the data */
    nummaterials = 0;
    while(fscanf(file, "%s", buf) != EOF) {
        switch(buf[0]) {
			case '#':               /* comment */
				/* eat up rest of line */
				fgets(buf, sizeof(buf), file);
				break;
			case 'n':               /* newmtl */
				fgets(buf, sizeof(buf), file);
				sscanf(buf, "%s %s", buf, buf);
				nummaterials++;
				materials[nummaterials].name = buf;
				break;
			case 'N':
				fscanf(file, "%f", &dummy);
				//// wavefront shininess is from [0, 1000], so scale for OpenGL
				//model->materials[nummaterials].shininess /= 1000.0;
				//model->materials[nummaterials].shininess *= 128.0;
				break;
			case 'K':
				switch(buf[1]) {
					case 'd':
						fscanf(file, "%f %f %f",
							   materials[nummaterials].diffuse.GetPtr()+0,
							   materials[nummaterials].diffuse.GetPtr()+1,
							   materials[nummaterials].diffuse.GetPtr()+2);
						break;
					case 's':
						// specular ignored
						fscanf(file, "%f %f %f", &dummy, &dummy, &dummy);
						break;
					case 'a':
						fscanf(file, "%f %f %f",
							   materials[nummaterials].ambient.GetPtr()+0,
							   materials[nummaterials].ambient.GetPtr()+1,
							   materials[nummaterials].ambient.GetPtr()+2);
						break;
					default:
						/* eat up rest of line */
						fgets(buf, sizeof(buf), file);
						break;
				}
				break;
            default:
                /* eat up rest of line */
                fgets(buf, sizeof(buf), file);
                break;
        }
    }
	
	return true;
}


/* glmFirstPass: first pass at a Wavefront OBJ file that gets all the
 * statistics of the model (such as #vertices, #normals, etc)
 *
 * model - properly initialized ImportOBJState structure
 * file  - (fopen'd) file descriptor 
 */
static void
glmFirstPass(ImportOBJState* model, FILE* file) 
{
    UInt32  numvertices;        /* number of vertices in model */
    UInt32  numnormals;         /* number of normals in model */
    UInt32  numtexcoords;       /* number of texcoords in model */
    UInt32  numtriangles;       /* number of triangles in model */
    GLMgroup* group;            /* current group */
    unsigned    v, n, t;
    char        buf[128];
    
    /* make a default group */
    group = glmAddGroup(model, "default");
    
    numvertices = numnormals = numtexcoords = numtriangles = 0;
    while(fscanf(file, "%s", buf) != EOF) {
        switch(buf[0]) {
			case '#':               /* comment */
				/* eat up rest of line */
				fgets(buf, sizeof(buf), file);
				break;
			case 'v':               /* v, vn, vt */
				switch(buf[1]) {
					case '\0':          /* vertex */
						/* eat up rest of line */
						fgets(buf, sizeof(buf), file);
						numvertices++;
						break;
					case 'n':           /* normal */
						/* eat up rest of line */
						fgets(buf, sizeof(buf), file);
						numnormals++;
						break;
					case 't':           /* texcoord */
						/* eat up rest of line */
						fgets(buf, sizeof(buf), file);
						numtexcoords++;
						break;
					default:
						printf_console("glmFirstPass(): Unknown token \"%s\".\n", buf);
						exit(1);
						break;
				}
				break;
            case 'm':
                fgets(buf, sizeof(buf), file);
                sscanf(buf, "%s %s", buf, buf);
                model->mtllibname = buf;
                glmReadMTL(model, buf);
                break;
            case 'u':
                /* eat up rest of line */
                fgets(buf, sizeof(buf), file);
                break;
            case 'g':               /* group */
                /* eat up rest of line */
                fgets(buf, sizeof(buf), file);
#if SINGLE_STRING_GROUP_NAMES
                sscanf(buf, "%s", buf);
#else
                buf[strlen(buf)-1] = '\0';  /* nuke '\n' */
#endif
                group = glmAddGroup(model, buf);
                break;
            case 'f':               /* face */
                v = n = t = 0;
                fscanf(file, "%s", buf);
                /* can be one of %d, %d//%d, %d/%d, %d/%d/%d %d//%d */
                if (strstr(buf, "//")) {
                    /* v//n */
                    sscanf(buf, "%d//%d", &v, &n);
                    fscanf(file, "%d//%d", &v, &n);
                    fscanf(file, "%d//%d", &v, &n);
                    numtriangles++;
                    group->triangles.push_back(0);
                    while(fscanf(file, "%d//%d", &v, &n) > 0) {
                        numtriangles++;
						group->triangles.push_back(0);
                    }
                } else if (sscanf(buf, "%d/%d/%d", &v, &t, &n) == 3) {
                    /* v/t/n */
                    fscanf(file, "%d/%d/%d", &v, &t, &n);
                    fscanf(file, "%d/%d/%d", &v, &t, &n);
                    numtriangles++;
                    group->triangles.push_back(0);
                    while(fscanf(file, "%d/%d/%d", &v, &t, &n) > 0) {
                        numtriangles++;
						group->triangles.push_back(0);
                    }
                } else if (sscanf(buf, "%d/%d", &v, &t) == 2) {
                    /* v/t */
                    fscanf(file, "%d/%d", &v, &t);
                    fscanf(file, "%d/%d", &v, &t);
                    numtriangles++;
                    group->triangles.push_back(0);
                    while(fscanf(file, "%d/%d", &v, &t) > 0) {
                        numtriangles++;
						group->triangles.push_back(0);
                    }
                } else {
                    /* v */
                    fscanf(file, "%d", &v);
                    fscanf(file, "%d", &v);
                    numtriangles++;
                    group->triangles.push_back(0);
                    while(fscanf(file, "%d", &v) > 0) {
                        numtriangles++;
						group->triangles.push_back(0);
                    }
                }
                break;
                
				default:
                /* eat up rest of line */
                fgets(buf, sizeof(buf), file);
                break;
        }
	}
	
	/* set the stats in the model structure */
	model->numvertices  = numvertices;
	model->numnormals   = numnormals;
	model->numtexcoords = numtexcoords;
	model->numtriangles = numtriangles;
}

/* glmSecondPass: second pass at a Wavefront OBJ file that gets all
 * the data.
 *
 * model - properly initialized ImportOBJState structure
 * file  - (fopen'd) file descriptor 
 */
static void
glmSecondPass(ImportOBJState* model, FILE* file) 
{
    UInt32  numvertices;        /* number of vertices in model */
    UInt32  numnormals;         /* number of normals in model */
    UInt32  numtexcoords;       /* number of texcoords in model */
    UInt32  numtriangles;       /* number of triangles in model */
    float*    vertices;           /* array of vertices  */
    float*    normals;            /* array of normals */
    float*    texcoords;          /* array of texture coordinates */
    GLMgroup* group;            /* current group pointer */
    UInt32  material;           /* current material */
    UInt32  v, n, t;
    char        buf[128];
    
    /* set the pointer shortcuts */
    vertices       = model->vertices;
    normals    = model->normals;
    texcoords    = model->texcoords;
    group      = model->groups;
    
    /* on the second pass through the file, read all the data into the
	 allocated arrays */
    numvertices = numnormals = numtexcoords = 1;
    numtriangles = 0;
    material = 0;
    while(fscanf(file, "%s", buf) != EOF) {
        switch(buf[0]) {
			case '#':               /* comment */
				/* eat up rest of line */
				fgets(buf, sizeof(buf), file);
				break;
			case 'v':               /* v, vn, vt */
				switch(buf[1]) {
					case '\0':          /* vertex */
						fscanf(file, "%f %f %f", 
							   &vertices[3 * numvertices + 0], 
							   &vertices[3 * numvertices + 1], 
							   &vertices[3 * numvertices + 2]);
						numvertices++;
						break;
					case 'n':           /* normal */
						fscanf(file, "%f %f %f", 
							   &normals[3 * numnormals + 0],
							   &normals[3 * numnormals + 1], 
							   &normals[3 * numnormals + 2]);
						numnormals++;
						break;
					case 't':           /* texcoord */
						fscanf(file, "%f %f", 
							   &texcoords[2 * numtexcoords + 0],
							   &texcoords[2 * numtexcoords + 1]);
						numtexcoords++;
						break;
				}
				break;
            case 'u':
                fgets(buf, sizeof(buf), file);
                sscanf(buf, "%s %s", buf, buf);
                group->material = material = glmFindMaterial(model, buf);
                break;
            case 'g':               /* group */
                /* eat up rest of line */
                fgets(buf, sizeof(buf), file);
#if SINGLE_STRING_GROUP_NAMES
                sscanf(buf, "%s", buf);
#else
                buf[strlen(buf)-1] = '\0';  /* nuke '\n' */
#endif
                group = glmFindGroup(model, buf);
                group->material = material;
                break;
            case 'f':               /* face */
                v = n = t = 0;
                fscanf(file, "%s", buf);
                /* can be one of %d, %d//%d, %d/%d, %d/%d/%d %d//%d */
                if (strstr(buf, "//")) {
                    /* v//n */
                    sscanf(buf, "%d//%d", &v, &n);
                    T(numtriangles).vindices[0] = v;
                    T(numtriangles).nindices[0] = n;
                    fscanf(file, "%d//%d", &v, &n);
                    T(numtriangles).vindices[1] = v;
                    T(numtriangles).nindices[1] = n;
                    fscanf(file, "%d//%d", &v, &n);
                    T(numtriangles).vindices[2] = v;
                    T(numtriangles).nindices[2] = n;
                    group->triangles[group->triindex++] = numtriangles;
                    numtriangles++;
                    while(fscanf(file, "%d//%d", &v, &n) > 0) {
                        T(numtriangles).vindices[0] = T(numtriangles-1).vindices[0];
                        T(numtriangles).nindices[0] = T(numtriangles-1).nindices[0];
                        T(numtriangles).vindices[1] = T(numtriangles-1).vindices[2];
                        T(numtriangles).nindices[1] = T(numtriangles-1).nindices[2];
                        T(numtriangles).vindices[2] = v;
                        T(numtriangles).nindices[2] = n;
                        group->triangles[group->triindex++] = numtriangles;
                        numtriangles++;
                    }
                } else if (sscanf(buf, "%d/%d/%d", &v, &t, &n) == 3) {
                    /* v/t/n */
                    T(numtriangles).vindices[0] = v;
                    T(numtriangles).tindices[0] = t;
                    T(numtriangles).nindices[0] = n;
                    fscanf(file, "%d/%d/%d", &v, &t, &n);
                    T(numtriangles).vindices[1] = v;
                    T(numtriangles).tindices[1] = t;
                    T(numtriangles).nindices[1] = n;
                    fscanf(file, "%d/%d/%d", &v, &t, &n);
                    T(numtriangles).vindices[2] = v;
                    T(numtriangles).tindices[2] = t;
                    T(numtriangles).nindices[2] = n;
                    group->triangles[group->triindex++] = numtriangles;
                    numtriangles++;
                    while(fscanf(file, "%d/%d/%d", &v, &t, &n) > 0) {
                        T(numtriangles).vindices[0] = T(numtriangles-1).vindices[0];
                        T(numtriangles).tindices[0] = T(numtriangles-1).tindices[0];
                        T(numtriangles).nindices[0] = T(numtriangles-1).nindices[0];
                        T(numtriangles).vindices[1] = T(numtriangles-1).vindices[2];
                        T(numtriangles).tindices[1] = T(numtriangles-1).tindices[2];
                        T(numtriangles).nindices[1] = T(numtriangles-1).nindices[2];
                        T(numtriangles).vindices[2] = v;
                        T(numtriangles).tindices[2] = t;
                        T(numtriangles).nindices[2] = n;
                        group->triangles[group->triindex++] = numtriangles;
                        numtriangles++;
                    }
                } else if (sscanf(buf, "%d/%d", &v, &t) == 2) {
                    /* v/t */
                    T(numtriangles).vindices[0] = v;
                    T(numtriangles).tindices[0] = t;
                    fscanf(file, "%d/%d", &v, &t);
                    T(numtriangles).vindices[1] = v;
                    T(numtriangles).tindices[1] = t;
                    fscanf(file, "%d/%d", &v, &t);
                    T(numtriangles).vindices[2] = v;
                    T(numtriangles).tindices[2] = t;
                    group->triangles[group->triindex++] = numtriangles;
                    numtriangles++;
                    while(fscanf(file, "%d/%d", &v, &t) > 0) {
                        T(numtriangles).vindices[0] = T(numtriangles-1).vindices[0];
                        T(numtriangles).tindices[0] = T(numtriangles-1).tindices[0];
                        T(numtriangles).vindices[1] = T(numtriangles-1).vindices[2];
                        T(numtriangles).tindices[1] = T(numtriangles-1).tindices[2];
                        T(numtriangles).vindices[2] = v;
                        T(numtriangles).tindices[2] = t;
                        group->triangles[group->triindex++] = numtriangles;
                        numtriangles++;
                    }
                } else {
                    /* v */
                    sscanf(buf, "%d", &v);
                    T(numtriangles).vindices[0] = v;
                    fscanf(file, "%d", &v);
                    T(numtriangles).vindices[1] = v;
                    fscanf(file, "%d", &v);
                    T(numtriangles).vindices[2] = v;
                    group->triangles[group->triindex++] = numtriangles;
                    numtriangles++;
                    while(fscanf(file, "%d", &v) > 0) {
                        T(numtriangles).vindices[0] = T(numtriangles-1).vindices[0];
                        T(numtriangles).vindices[1] = T(numtriangles-1).vindices[2];
                        T(numtriangles).vindices[2] = v;
                        group->triangles[group->triindex++] = numtriangles;
                        numtriangles++;
                    }
                }
                break;
                
				default:
                /* eat up rest of line */
                fgets(buf, sizeof(buf), file);
                break;
		}
	}	
}




/* glmReverseWinding: Reverse the polygon winding for all polygons in
 * this model.   Default winding is counter-clockwise.  Also changes
 * the direction of the normals.
 * 
 * model - properly initialized ImportOBJState structure 
 */
void
glmReverseWinding(ImportOBJState* model)
{
    UInt32 i, swap;
    
    AssertIf( !model );
    
    for (i = 0; i < model->numtriangles; i++) {
        swap = T(i).vindices[0];
        T(i).vindices[0] = T(i).vindices[2];
        T(i).vindices[2] = swap;
        
        if (model->numnormals) {
            swap = T(i).nindices[0];
            T(i).nindices[0] = T(i).nindices[2];
            T(i).nindices[2] = swap;
        }
        
        if (model->numtexcoords) {
            swap = T(i).tindices[0];
            T(i).tindices[0] = T(i).tindices[2];
            T(i).tindices[2] = swap;
        }
    }
    
    /* reverse vertex normals */
    for (i = 1; i <= model->numnormals; i++) {
        model->normals[3 * i + 0] = -model->normals[3 * i + 0];
        model->normals[3 * i + 1] = -model->normals[3 * i + 1];
        model->normals[3 * i + 2] = -model->normals[3 * i + 2];
    }
}


bool ImportModelOBJ( const std::string& pathname, ImportScene* scene )
{
    /* open the file */
    FILE* file = fopen(pathname.c_str(), "rt");
    if (!file) {
		return false;
    }
    
    /* allocate a new model */
    ImportOBJState model;
	model.m_Scene = scene;
    model.pathname    = pathname;
    model.numvertices   = 0;
    model.vertices    = NULL;
    model.numnormals    = 0;
    model.normals     = NULL;
    model.numtexcoords  = 0;
    model.texcoords       = NULL;
    model.numtriangles  = 0;
    model.triangles       = NULL;
    model.numgroups       = 0;
    model.groups      = NULL;
    model.position[0]   = 0.0;
    model.position[1]   = 0.0;
    model.position[2]   = 0.0;

    // make a first pass through the file to get a count of the number of vertices, normals, texcoords & triangles
    glmFirstPass( &model, file );
    
    /* allocate memory */
    model.vertices = (float*)malloc(sizeof(float) *
									   3 * (model.numvertices + 1));
    model.triangles = (GLMtriangle*)malloc(sizeof(GLMtriangle) *
											model.numtriangles);
    if (model.numnormals) {
        model.normals = (float*)malloc(sizeof(float) *
										  3 * (model.numnormals + 1));
    }
    if (model.numtexcoords) {
        model.texcoords = (float*)malloc(sizeof(float) *
											2 * (model.numtexcoords + 1));
    }
    
    /* rewind to beginning of file and read in the data this pass */
    rewind(file);
    
    glmSecondPass( &model, file );
    
    /* close the file */
    fclose(file);
	
    if (model.vertices)     free(model.vertices);
    if (model.normals)  free(model.normals);
    if (model.texcoords)  free(model.texcoords);
    if (model.triangles)  free(model.triangles);
    while(model.groups) {
        GLMgroup* group = model.groups;
        model.groups = model.groups->next;
        delete group;
    }
    
    return true;
}
