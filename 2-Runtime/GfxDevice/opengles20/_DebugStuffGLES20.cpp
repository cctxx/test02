#include "UnityPrefix.h"
#include "AssertGLES20.h"
#include "IncludesGLES20.h"
#include "Runtime/Math/Matrix4x4.h"
#include "_DebugStuffGLES20.h"
#include "VBOGLES20.h"

void PrintMatrix(const char* name, const Matrix4x4f& m)
{
	printf_console("Outputing:%s\n------------\n"
		"mat4 mat = (%.2f, %.2f, %.2f, %2f,\n"
		"%.2f, %.2f, %.2f, %2f,\n"
		"%.2f, %.2f, %.2f, %2f,\n"
		"%.2f, %.2f, %.2f, %2f);\n\n",name,
		m[0],	m[1],	m[2],	m[3],
		m[4],	m[5],	m[6],	m[7],
		m[8],	m[9],	m[10],	m[11],
		m[12],	m[13],	m[14],	m[15]);
}
void _Debug::MiniLoopGLES20()
{

		#define VERTEX_ARRAY	0
	float pfIdentity[] =
	{
		1.0f,0.0f,0.0f,0.0f,
		0.0f,1.0f,0.0f,0.0f,
		0.0f,0.0f,1.0f,0.0f,
		0.0f,0.0f,0.0f,1.0f
	};

	// Fragment and vertex shaders code
	char* pszFragShader = "\
		void main (void)\
		{\
			gl_FragColor = vec4(1.0, 1.0, 0.66 ,1.0);\
		}";
	char* pszVertShader = "\
		attribute highp vec3	myVertex;\
		uniform mediump mat4	myPMVMatrix;\
		void main(void)\
		{\
		gl_Position = myPMVMatrix * vec4(myVertex,1.0);\
		}";


		GLuint uiFragShader, uiVertShader;		/* Used to hold the fragment and vertex shader handles */
	GLuint uiProgramObject;					/* Used to hold the program handle (made out of the two previous shaders */

	// Create the fragment shader object
	uiFragShader = glCreateShader(GL_FRAGMENT_SHADER);

	// Load the source code into it
	glShaderSource(uiFragShader, 1, (const char**)&pszFragShader, NULL);

	// Compile the source code
	glCompileShader(uiFragShader);

	// Check if compilation succeeded
	GLint bShaderCompiled;
	bShaderCompiled = 0;
    glGetShaderiv(uiFragShader, GL_COMPILE_STATUS, &bShaderCompiled);

	if (!bShaderCompiled)
	{
#ifndef NO_GDI
		// An error happened, first retrieve the length of the log message
		int i32InfoLogLength, i32CharsWritten;
		glGetShaderiv(uiFragShader, GL_INFO_LOG_LENGTH, &i32InfoLogLength);

		// Allocate enough space for the message and retrieve it
		char* pszInfoLog = new char[i32InfoLogLength];
        glGetShaderInfoLog(uiFragShader, i32InfoLogLength, &i32CharsWritten, pszInfoLog);

		printf_console(pszInfoLog);
		// Displays the error in a dialog box
		delete[] pszInfoLog;
#endif
		return;
	}

	// Loads the vertex shader in the same way
	uiVertShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(uiVertShader, 1, (const char**)&pszVertShader, NULL);
	glCompileShader(uiVertShader);
    glGetShaderiv(uiVertShader, GL_COMPILE_STATUS, &bShaderCompiled);
	if (!bShaderCompiled)
	{
#ifndef NO_GDI
		int i32InfoLogLength, i32CharsWritten;
		glGetShaderiv(uiVertShader, GL_INFO_LOG_LENGTH, &i32InfoLogLength);
		char* pszInfoLog = new char[i32InfoLogLength];
        glGetShaderInfoLog(uiVertShader, i32InfoLogLength, &i32CharsWritten, pszInfoLog);
		printf_console(pszInfoLog);
		delete[] pszInfoLog;
#endif
		return;
	}

	// Create the shader program
    uiProgramObject = glCreateProgram();
	// Bind the custom vertex attribute "myVertex" to location VERTEX_ARRAY
    glBindAttribLocation(uiProgramObject, VERTEX_ARRAY, "myVertex");
	// Attach the fragment and vertex shaders to it
    glAttachShader(uiProgramObject, uiFragShader);
    glAttachShader(uiProgramObject, uiVertShader);



	// Link the program
    glLinkProgram(uiProgramObject);

	// Check if linking succeeded in the same way we checked for compilation success
    GLint bLinked;
    glGetProgramiv(uiProgramObject, GL_LINK_STATUS, &bLinked);
	if (!bLinked)
	{
#ifndef NO_GDI
		int i32InfoLogLength, i32CharsWritten;
		glGetProgramiv(uiProgramObject, GL_INFO_LOG_LENGTH, &i32InfoLogLength);
		char* pszInfoLog = new char[i32InfoLogLength];
		glGetProgramInfoLog(uiProgramObject, i32InfoLogLength, &i32CharsWritten, pszInfoLog);
		printf_console(pszInfoLog);
		delete[] pszInfoLog;
#endif
		return;
	}


		// Actually use the created program
    glUseProgram(uiProgramObject);
	GLESAssert();

	// Sets the clear color.
	// The colours are passed per channel (red,green,blue,alpha) as float values from 0.0 to 1.0
	glClearColor(0.6f, 0.8f, 1.0f, 1.0f);
	GLESAssert();

	// We're going to draw a triangle to the screen so create a vertex buffer object for our triangle
	GLuint	ui32Vbo; // Vertex buffer object handle
	
	// Interleaved vertex data
	GLfloat afVertices[] = {	-0.1f,-0.4f,0.0f, // Position
								0.4f ,-0.4f,0.0f,
								0.0f ,0.4f ,0.0f};

	// Generate the vertex buffer object (VBO)
	glGenBuffers(1, &ui32Vbo);
	GLESAssert();

	// Bind the VBO so we can fill it with data
	glBindBuffer(GL_ARRAY_BUFFER, ui32Vbo);
	GLESAssert();

	// Set the buffer's data
	unsigned int uiSize = 3 * (sizeof(GLfloat) * 3); // Calc afVertices size (3 vertices * stride (3 GLfloats per vertex))
	glBufferData(GL_ARRAY_BUFFER, uiSize, afVertices, GL_STATIC_DRAW);
	GLESAssert();

	glClearDepthf(1.0f);									
	glEnable(GL_DEPTH_TEST);							
	glDepthFunc(GL_LEQUAL);	
	glDisable(GL_CULL_FACE);
	printf_console("Loop launched\n");
	while (1) 
	{
		glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		GLESAssert();


		/*
			Bind the projection model view matrix (PMVMatrix) to
			the associated uniform variable in the shader
		*/

		// First gets the location of that variable in the shader using its name
		int i32Location = glGetUniformLocation(uiProgramObject, "myPMVMatrix");

		// Then passes the matrix to that variable
		glUniformMatrix4fv( i32Location, 1, GL_FALSE, pfIdentity);
		GLESAssert();

		/*
			Enable the custom vertex attribute at index VERTEX_ARRAY.
			We previously binded that index to the variable in our shader "vec4 MyVertex;"
		*/
		glEnableVertexAttribArray(VERTEX_ARRAY);
		GLESAssert();

		// Sets the vertex data to this attribute index
		glVertexAttribPointer(VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, 0, 0);

		/*
			Draws a non-indexed triangle array from the pointers previously given.
			This function allows the use of other primitive types : triangle strips, lines, ...
			For indexed geometry, use the function glDrawElements() with an index list.
		*/
		glDrawArrays(GL_TRIANGLES, 0, 3);
		GLESAssert();

		void PresentContextGLES();
		PresentContextGLES();
   }
}
int	_Debug::CreateDefaultShader (bool texturing)
{
	char* strVertexShader = NULL;
	char* strFragmentShader = NULL;
	if (texturing)
	{
		strVertexShader = 
			"attribute highp vec3 _glesVertex;\n"
			"attribute highp vec4 _glesUV0;\n"
			"uniform mediump mat4 _glesMVP;\n"
			"varying mediump vec2 _glesVarUV0;\n"
			"void main(void)\n"
			"{\n"
			"	gl_Position = _glesMVP * vec4(_glesVertex,1.0);\n"
			"	_glesVarUV0 = _glesUV0.st;\n"
			"}";

		strFragmentShader = 
			"uniform sampler2D texDiffuse;\n"
			"varying mediump vec2 _glesVarUV0;\n"
			"void main (void)\n"
			"{\n"
			"	gl_FragColor = texture2D(texDiffuse, _glesVarUV0);\n"
			"}";
	}
	else
	{
		strVertexShader = 
			"attribute highp vec3 _glesVertex;\n"
			"uniform mediump mat4 _glesMVP;\n"
			"void main(void)\n"
			"{\n"
			"	gl_Position = _glesMVP * vec4(_glesVertex,1.0);\n"
			"}";


		strFragmentShader = 
			"void main (void)\n"
			"{\n"
			"	gl_FragColor = vec4(1.0, 1.0, 0.66 ,1.0);\n"
			"}";

	}
	return CreateShader(strVertexShader, strFragmentShader);

}
int	_Debug::CreateShader(const char* vertexShader, const char* fragmenShader)
{
	GLuint vertShader, fragShader;
	GLuint shader;

	GLint compiled, linked;
	
	//Vertex shader
	GLES_CHK(vertShader = glCreateShader(GL_VERTEX_SHADER));
	GLES_CHK(glShaderSource(vertShader, 1, (const char**)&vertexShader, NULL));
	GLES_CHK(glCompileShader(vertShader));

	compiled = 0;
    GLES_CHK(glGetShaderiv(vertShader, GL_COMPILE_STATUS, &compiled));

	if (!compiled)
	{
		int i32InfoLogLength, i32CharsWritten;
		GLES_CHK(glGetShaderiv(vertShader, GL_INFO_LOG_LENGTH, &i32InfoLogLength));
		char* pszInfoLog = new char[i32InfoLogLength];
        GLES_CHK(glGetShaderInfoLog(vertShader, i32InfoLogLength, &i32CharsWritten, pszInfoLog));
		printf_console(pszInfoLog);
		delete[] pszInfoLog;
		return 0;
	}

	//Fragment shader
	GLES_CHK(fragShader = glCreateShader(GL_FRAGMENT_SHADER));
	GLES_CHK(glShaderSource(fragShader, 1, (const char**)&fragmenShader, NULL));
	GLES_CHK(glCompileShader(fragShader));

	compiled = 0;
    GLES_CHK(glGetShaderiv(fragShader, GL_COMPILE_STATUS, &compiled));

	if (!compiled)
	{
		int i32InfoLogLength, i32CharsWritten;
		GLES_CHK(glGetShaderiv(fragShader, GL_INFO_LOG_LENGTH, &i32InfoLogLength));
		char* pszInfoLog = new char[i32InfoLogLength];
        GLES_CHK(glGetShaderInfoLog(fragShader, i32InfoLogLength, &i32CharsWritten, pszInfoLog));
		printf_console(pszInfoLog);
		delete[] pszInfoLog;
		return 0;
	}

	// Final Step, attacha and link shaders
    shader = glCreateProgram();
    (glBindAttribLocation(shader, GL_VERTEX_ARRAY, "_glesVertex"));
	(glBindAttribLocation(shader, GL_TEXTURE_ARRAY0, "_glesUV0"));

    GLES_CHK(glAttachShader(shader, fragShader));
    GLES_CHK(glAttachShader(shader, vertShader));
    GLES_CHK(glLinkProgram(shader));

	linked = 0;
    GLES_CHK(glGetProgramiv(shader, GL_LINK_STATUS, &linked));
	if (!linked)
	{
		int i32InfoLogLength, i32CharsWritten;
		GLES_CHK(glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &i32InfoLogLength));
		char* pszInfoLog = new char[i32InfoLogLength];
		GLES_CHK(glGetProgramInfoLog(shader, i32InfoLogLength, &i32CharsWritten, pszInfoLog));
		printf_console(pszInfoLog);
		delete[] pszInfoLog;
		return 0;
	}
	return shader;
}

int	_Debug::CreateDefaultVBO	(bool texCoords)
{
	float verts[] = 
	{
		-0.5f, -0.5f, 0.0f,
		 0.5f, -0.5f, 0.0f,
		 0.5f,  0.5f, 0.0f,
		-0.5f, -0.5f, 0.0f,
		 0.5f,  0.5f, 0.0f,
	    -0.5f,  0.5f, 0.0f
	};

	float vertsUV[] = 
	{
		-0.5f, -0.5f, 0.0f,
		 0.0f,  0.0f, 
		 0.5f, -0.5f, 0.0f,
		 1.0f,  0.0f, 
		 0.5f,  0.5f, 0.0f,
		 1.0f,  1.0f, 
		-0.5f, -0.5f, 0.0f,
		 0.0f,  0.0f, 
		 0.5f,  0.5f, 0.0f,
		 1.0f,  1.0f, 
	    -0.5f,  0.5f, 0.0f,
		 0.0f,  1.0f, 
	};

	if (texCoords)
	{
		return CreateVBO(vertsUV, 6, sizeof(float) * 5);
	}
	else
	{
		return CreateVBO(vertsUV, 6, sizeof(float) * 3);
	}
}


int	_Debug::CreateVBO(const float* data, int vertexCount, int vertexSize)
{
	GLuint vbo;
	GLES_CHK(glGenBuffers(1, &vbo));
	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, vbo));
	GLES_CHK(glBufferData(GL_ARRAY_BUFFER, vertexSize * vertexCount, data, GL_STATIC_DRAW));
	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
	return vbo;
}

void _Debug::SetUniformMatrix4x4f(int shaderID, const char* name, const Matrix4x4f& mat)
{
	int loc ;
	GLES_CHK(loc = glGetUniformLocation(shaderID, name));

	if (loc != -1)GLES_CHK(glUniformMatrix4fv(loc, 1, GL_FALSE, mat.GetPtr()));
	else printf_console("Failed to find uniform %s\n", name);
}
void _Debug::SetUniformSample2D  (int shaderID, const char* name, int texUnit)
{
	int loc ;
	GLES_CHK(loc = glGetUniformLocation(shaderID, name));

	if (loc != -1)GLES_CHK(glUniform1i(loc, texUnit));
	else printf_console("Failed to find uniform %s\n", name);
}
void _Debug::UseShader(int shaderID)
{
	GLES_CHK(glUseProgram(shaderID));
}
void _Debug::DrawVBO(int vboID, int vertexCount, bool texCoords)
{
	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, vboID));
	if (texCoords)
	{
		glEnableVertexAttribArray(GL_VERTEX_ARRAY);
		glVertexAttribPointer(GL_VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 5, 0);
		glEnableVertexAttribArray(GL_TEXTURE_ARRAY0);
		glVertexAttribPointer(GL_TEXTURE_ARRAY0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 5, (void*) (3 * sizeof(GLfloat)));

		GLES_CHK(glDrawArrays(GL_TRIANGLES, 0, vertexCount));
		
		GLES_CHK(glDisableVertexAttribArray(GL_VERTEX_ARRAY));
		GLES_CHK(glDisableVertexAttribArray(GL_TEXTURE_ARRAY0));
	}
	else
	{
		GLES_CHK(glEnableVertexAttribArray(GL_VERTEX_ARRAY));
		GLES_CHK(glVertexAttribPointer(VERTEX_ARRAY, 3, GL_FLOAT, GL_FALSE, 0, 0));
		GLES_CHK(glDrawArrays(GL_TRIANGLES, 0, vertexCount));
		GLES_CHK(glDisableVertexAttribArray(GL_VERTEX_ARRAY));
	}
	GLES_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
}
void _Debug::DisplayTextureMiniLoop(int textureID)
{
	bool texCoords = true;
	int vbo = CreateDefaultVBO(texCoords);
	int shader = CreateDefaultShader(texCoords);
	GLES_CHK(glDisable(GL_DEPTH_TEST));							
	GLES_CHK(glDisable(GL_CULL_FACE));
	GLES_CHK(glDisable(GL_BLEND));
	GLES_CHK(glBindFramebuffer (GL_FRAMEBUFFER, 0));
	while (1) 
	{
		GLES_CHK(glClearColor(1.0f, 1.0f, 0.0f, 1.0f));
		GLES_CHK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		GLES_CHK(glActiveTexture(GL_TEXTURE0));
		GLES_CHK(glBindTexture(GL_TEXTURE_2D, textureID));
		
		UseShader(shader);
		SetUniformMatrix4x4f(shader, "_glesMVP", Matrix4x4f::identity);
		SetUniformSample2D(shader, "texDiffuse", 0);
		DrawVBO(vbo, 6, texCoords);

		void PresentContextGLES();
		PresentContextGLES();
	}
}