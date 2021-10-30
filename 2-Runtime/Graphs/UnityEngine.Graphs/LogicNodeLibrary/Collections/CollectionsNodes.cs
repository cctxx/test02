using System;
using System.Collections.Generic;

namespace UnityEngine.Graphs.LogicGraph
{
	public class CollectionsNodes
	{
		public enum IteratorIn { Reset, Next }
		public delegate void IteratorOut<T> (T element, int index);
		public delegate void IteratorElementOut<T>(T element);

		[Logic(null, typeof(IteratorIn), typeof(int))]
		[Title("Collections/Iterator")]
		public static void Iterator<T> (IteratorIn input, [DefaultValue(-1)] ref int index, List<T> collection, IteratorOut<T> resetOut, IteratorOut<T> iteration, IteratorOut<T> done)
		{
			if (input == IteratorIn.Reset)
			{
				index = -1;
				if (resetOut != null) resetOut (default(T), index);
				return;
			}

			if (++index >= collection.Count)
			{
				if (done != null) done (default (T), index);
			}
			else
			{
				if (iteration != null) iteration (collection[index], index);
				if (index + 1 >= collection.Count)
					if (done != null) done (default (T), index);
			}
		}

		[Logic]
		[Title("Collections/IterateAll")]
		public static void IterateAll<T> (List<T> collection, IteratorOut<T> iteration, IteratorOut<T> done)
		{
			for (int index = 0; index < collection.Count; ++index)
				if (iteration != null) iteration (collection[index], index);

			if (done != null) done (default (T), collection.Count - 1);
		}

		[Logic]
		[Title("Collections/Add")]
		public static void Add<T> (List<T> collection, T objectToAdd)
		{
			collection.Add (objectToAdd);
		}

		[Logic]
		[Title("Collections/Insert")]
		public static void Insert<T> (List<T> collection, T objectToAdd, int index)
		{
			collection.Insert (index, objectToAdd);
		}

		[Logic]
		[Title("Collections/GetElementAt")]
		public static void GetElementAt<T>(List<T> collection, int index, IteratorElementOut<T> success, IteratorElementOut<T> notPresent)
		{
			if (index < 0 || collection.Count <= index)
			{
				if (notPresent != null)
					notPresent ((T) (typeof (T).IsValueType ? Activator.CreateInstance (typeof (T)) : null));
			}
			else
				if (success != null)
					success(collection[index]);
		}

		[Logic]
		[Title("Collections/SetElementAt")]
		public static void SetElementAt<T>(List<T> collection, T element, int index, Action success, Action notPresent)
		{
			if (index < 0 || collection.Count <= index)
			{
				if (notPresent != null)
					notPresent();
			}
			else
			{
				collection[index] = element;

				if (success != null)
					success ();
			}
		}

		[Logic]
		[Title("Collections/Contains")]
		public static void Contains<T> (List<T> collection, T objectToTest, Action present, Action notPresent)
		{
			if (collection.Contains (objectToTest))
			{
				if (present != null) present ();
			}
			else if (notPresent != null) notPresent ();
		}

		[Logic]
		[Title("Collections/Remove")]
		public static void Remove<T> (List<T> collection, T objectToRemove, Action removed, Action notPresent)
		{
			if (collection.Remove (objectToRemove))
			{
				if (removed != null) removed ();
			}
			else if (notPresent != null) notPresent ();
		}

		[Logic]
		[Title("Collections/RemoveAt")]
		public static void RemoveAt<T> (List<T> collection, int index)
		{
			collection.RemoveAt (index);
		}

		[Logic]
		[Title("Collections/Clear")]
		public static void Clear<T> (List<T> collection)
		{
			collection.Clear ();
		}
	}
}

