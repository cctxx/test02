//Hackish solution to have UnityEditor actually depend on NUnit.Framework.dll, because when we don't
//use it, gmcs.exe actually "optimize" the reference away, and we want it to be available to user generated editor code (and testcode)
internal class NUnitFrameworkUser
{
   void Useit()
   {
     NUnit.Framework.Assert.AreEqual(1,1);
   }
}
