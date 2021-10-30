using System;
using System.Collections.Generic;
using System.Text;
using System.IO;

namespace UnityEditor
{
    internal class HeapshotReader
    {
        const uint kMagicNumber = 0x4eabfdd1;
        const int kLogVersion = 6;
        const string kLogFileLabel = "heap-shot logfile";

        public enum Tag
        {
            Type = 0x01,
            Object = 0x02,
            UnityObjects = 0x03,
            EndOfFile = 0xff
        }

        public enum ObjectType
        {
            None,
            Root,
            Managed,
            UnityRoot
        }

        public class FieldInfo
        {
            public string name = "";
            public FieldInfo()
            {
            }
            public FieldInfo(string name)
            {
                this.name = name;
            }
        }
        public class TypeInfo
        {
            public string name = "";
            public Dictionary<uint, FieldInfo> fields = new Dictionary<uint, FieldInfo>();
            public TypeInfo()
            {
            }
            public TypeInfo(string name)
            {
                this.name = name;
            }
        }

        public class ReferenceInfo
        {
            public uint code = 0;
            public ObjectInfo referencedObject = null;
            public FieldInfo fieldInfo = null;
            public ReferenceInfo()
            {

            }
            public ReferenceInfo(ObjectInfo refObj, FieldInfo field)
            {
                referencedObject = refObj;
                fieldInfo = field;
            }
        }
        public class BackReferenceInfo
        {
            public ObjectInfo parentObject = null;
            public FieldInfo fieldInfo = null;
        }

        public class ObjectInfo
        {
            public uint code = 0;
            public TypeInfo typeInfo = null;
            public uint size = 0;
            public List<ReferenceInfo> references = new List<ReferenceInfo>();
            public List<BackReferenceInfo> inverseReferences = new List<BackReferenceInfo>();
            public ObjectType type = ObjectType.None;
            public ObjectInfo()
            {
            }
            public ObjectInfo(TypeInfo typeInfo, uint size)
            {
                this.typeInfo = typeInfo;
                this.size = size;
            }
            public ObjectInfo(TypeInfo typeInfo, uint size, ObjectType type)
            {
                this.typeInfo = typeInfo;
                this.size = size;
                this.type = type;
            }
        }

        private Dictionary<uint, TypeInfo> types = new Dictionary<uint, TypeInfo>();
        private Dictionary<uint, ObjectInfo> objects = new Dictionary<uint, ObjectInfo>();
        private List<ReferenceInfo> roots = new List<ReferenceInfo>();
        private List<ObjectInfo> allObjects = new List<ObjectInfo>();
        private List<TypeInfo> allTypes = new List<TypeInfo>();
        private ObjectInfo kUnmanagedObject = new ObjectInfo(new TypeInfo("Unmanaged"), 0);
        private ObjectInfo kUnity = new ObjectInfo(new TypeInfo("<Unity>"), 0, ObjectType.UnityRoot);
        //private FieldInfo kEmptyUnityField = new FieldInfo("");

        public HeapshotReader()
        {

        }

        public List<ReferenceInfo> Roots
        {
            get
            {
                return roots;
            }
        }
        public List<ObjectInfo> Objects
        {
            get
            {
                return allObjects;
            }
        }

        public List<TypeInfo> Types
        {
            get
            {
                return allTypes;
            }
        }

        public List<ObjectInfo> GetObjectsOfType(string name)
        {
            List<ObjectInfo> objs = new List<ObjectInfo>();
            foreach (ObjectInfo o in allObjects)
            {
                if (o.typeInfo.name == name)
                {
                    objs.Add(o);
                }
            }
            return objs;
        }

        public bool Open(string fileName)
        {
            types.Clear();
            objects.Clear();
            roots.Clear();
            allObjects.Clear();
            allTypes.Clear();

            Stream stream;
            try
            {
                stream = new FileStream(fileName, FileMode.Open, FileAccess.Read);
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                return false;
            }

            BinaryReader reader;
            try
            {
                reader = new BinaryReader(stream);
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
                stream.Close();
                return false;
            }

            ReadHeader(reader);
            while (ReadData(reader))
            {
            }

            ResolveReferences();

            ResolveInverseReferences();

            ResolveRoots();

            // For roots add a special backtrace object called 'Root'
            /*
            foreach (ObjectInfo o in roots)
            {
                o.isRoot = true;
                ObjectInfo targetObject = o.references.Count == 1 ? o.references[0].referencedObject : o;
    
                BackReferenceInfo newBackTraceInfo = new BackReferenceInfo();
                newBackTraceInfo.parentObject = kRoot;
                newBackTraceInfo.fieldInfo = kEmptyRootField;
                targetObject.inverseReferences.Add(newBackTraceInfo);
            }
             */

            reader.Close();
            stream.Close();

            return true;
        }
        private void ReadHeader(BinaryReader reader)
        {
            uint magicNumber;
            magicNumber = reader.ReadUInt32();
            if (magicNumber != kMagicNumber)
            {
                throw new Exception(string.Format("Bad magic number: expected {0}, found {1}", kMagicNumber, magicNumber));
            }

            int version;
            version = reader.ReadInt32();

            string label;
            int logVersion;

            label = reader.ReadString();
            if (label == kLogFileLabel)
            {
                logVersion = kLogVersion;
            }
            else
            {
                throw new Exception("Unknown file label in heap-shot outfile");
            }

            if (version != logVersion)
            {
                throw new Exception(String.Format("Version error in {0}: expected {1}, found {2}", label, logVersion, version));
            }
            /*
            uint numTypes = reader.ReadUInt32();
            uint numObjects = reader.ReadUInt32();
            uint numReferences = reader.ReadUInt32();
            uint numFields = reader.ReadUInt32();
             * */

            reader.ReadUInt32();
            reader.ReadUInt32();
            reader.ReadUInt32();
            reader.ReadUInt32();
        }
        private bool ReadData(BinaryReader reader)
        {
            Tag tag = (Tag)reader.ReadByte();
            switch (tag)
            {
                case Tag.Type:
                    ReadType(reader);
                    break;
                case Tag.Object:
                    ReadObject(reader);
                    break;
                case Tag.UnityObjects:
                    ReadUnityObjects(reader);
                    break;
                case Tag.EndOfFile:
                    return false;
                default:
                    throw new Exception("Unknown tag! " + tag);
            }

            return true;
        }
        private void ReadType(BinaryReader reader)
        {
            uint typeCode = reader.ReadUInt32();

            TypeInfo newType = new TypeInfo();
            newType.name = reader.ReadString();

            uint fieldCode;
            while ((fieldCode = reader.ReadUInt32()) != 0)
            {
                FieldInfo newField = new FieldInfo();
                newField.name = reader.ReadString();
                newType.fields[fieldCode] = newField;
            }
            if (types.ContainsKey(typeCode))
            {
                throw new Exception(string.Format("Type info for object {0} was already loaded!!!", typeCode));
            }
            else
            {
                types[typeCode] = newType;
                allTypes.Add(newType);
            }
        }
        private void ReadObject(BinaryReader reader)
        {
            uint objectCode = reader.ReadUInt32();
            uint typeCode = reader.ReadUInt32();

            ObjectInfo newObject = new ObjectInfo();
            newObject.code = objectCode;
            newObject.size = reader.ReadUInt32();

            if (types.ContainsKey(typeCode))
            {
                newObject.typeInfo = types[typeCode];
            }
            else
            {
                throw new Exception(string.Format("Failed to find type info {0} for object {1}!!!", typeCode, objectCode));
            }

            uint referenceCode;
            while ((referenceCode = reader.ReadUInt32()) != 0)
            {
                ReferenceInfo newReference = new ReferenceInfo();
                // Object might not be loaded yet, so temporarary store the code, and resolve references later
                newReference.code = referenceCode;
                uint fieldCode = reader.ReadUInt32();

                if (fieldCode == 0)
                {
                    // It means that our object is an array
                    newReference.fieldInfo = null;
                }
                else if (newObject.typeInfo.fields.ContainsKey(fieldCode))
                {
                    newReference.fieldInfo = newObject.typeInfo.fields[fieldCode];
                }
                else
                {
                    // Unmanaged type?
                    newReference.fieldInfo = null;
                    //throw new Exception(string.Format("Failed to find type info {0} for object's {1} reference {2}!!!", typeCode, objectCode, referenceCode));
                }

                newObject.references.Add(newReference);
            }

            if (objects.ContainsKey(objectCode))
            {
                throw new Exception(string.Format("Object {0} was already loaded?!", objectCode));
            }
            newObject.type = objectCode == typeCode ? ObjectType.Root : ObjectType.Managed;
            objects[objectCode] = newObject;
            allObjects.Add(newObject);
        }

        private void ReadUnityObjects(BinaryReader reader)
        {
            uint referenceCode;
            while ((referenceCode = reader.ReadUInt32()) != 0)
            {
                if (objects.ContainsKey(referenceCode))
                {
                    BackReferenceInfo newReference = new BackReferenceInfo();
                    newReference.parentObject = kUnity;
                    newReference.fieldInfo = new FieldInfo(objects[referenceCode].typeInfo.name);
                    objects[referenceCode].inverseReferences.Add(newReference);
                }
                else
                {
                    // throw new Exception(string.Format("Object {0} was already loaded?!", referenceCode));
                }
            }
        }


        private void ResolveReferences()
        {
            foreach (KeyValuePair<uint, ObjectInfo> o in objects)
            {
                foreach (ReferenceInfo reference in o.Value.references)
                {
                    if (objects.ContainsKey(reference.code) == false)
                    {
                        reference.referencedObject = kUnmanagedObject;
                    }
                    else
                    {
                        reference.referencedObject = objects[reference.code];

                        // If it's an array let's fill the name
                        if (reference.fieldInfo == null)
                        {
                            reference.fieldInfo = new FieldInfo();
                            reference.fieldInfo.name = reference.referencedObject.typeInfo.name;
                        }
                    }
                }
            }
        }

        private void ResolveInverseReferences()
        {
            foreach (KeyValuePair<uint, ObjectInfo> o in objects)
            {
                foreach (ReferenceInfo reference in o.Value.references)
                {
                    BackReferenceInfo backReferenceInfo = new BackReferenceInfo();
                    backReferenceInfo.parentObject = o.Value;
                    backReferenceInfo.fieldInfo = reference.fieldInfo;
                    reference.referencedObject.inverseReferences.Add(backReferenceInfo);
                }
            }
        }
        private void ResolveRoots()
        {
            foreach (KeyValuePair<uint, ObjectInfo> o in objects)
            {
                if (o.Value.type == ObjectType.Root)
                {
                    foreach (ReferenceInfo reference in o.Value.references)
                    {
                        roots.Add(reference);
                    }
                }
            }
        }
    }

}
