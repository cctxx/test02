namespace UnityEditor.VersionControl
{
public partial class ChangeSet
{
	public ChangeSet()
	{
		InternalCreate();
	}

	public ChangeSet(string description)
	{
		InternalCreateFromString(description);
	}

	public ChangeSet(string description, string revision)
	{
		InternalCreateFromStringString(description, revision);
	}

	public ChangeSet(ChangeSet other)
	{
		InternalCopyConstruct(other);
	}

	~ChangeSet()
	{
		Dispose ();
	}

}
}
