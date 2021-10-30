/*
 *  AssetLabels.h
 *  Xcode
 *
 *  Created by Hrafnkell Hlodversson on 2009-11-16.
 *  Copyright 2009 Unity Technologies ApS. All rights reserved.
 *
 */

#ifndef ASSETLABELS_H
#define ASSETLABELS_H

#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Serialize/SerializeUtility.h"

class AssetLabels
{
private:
	std::vector<UnityStr> m_Labels;
	UnityStr m_PartialAcc;
	
	void UpdatePartialMatchIndex();
public:
	AssetLabels() : m_Labels(), m_PartialAcc() {}
	
	const std::vector<UnityStr>& GetLabels() const { return m_Labels; } 
	
	void ClearLabels() { m_Labels.clear(); m_PartialAcc.clear(); } 
	
	template <class InputIterator>
	void AddLabels(InputIterator f, InputIterator l) ;
	
	template <class InputIterator>
	void RemoveLabels(InputIterator f, InputIterator l);
	
	template <class InputIterator>
	void SetLabels(InputIterator f, InputIterator l) ;
	
	bool Match(const UnityStr& label) const;
	
	AssetLabels& operator=(const AssetLabels& rhs)
	{
      m_Labels = rhs.m_Labels;
	  UpdatePartialMatchIndex();
      return *this;
	}

	DECLARE_SERIALIZE_NO_PPTR (AssetLabels)
	
};

template <class InputIterator>
void AssetLabels::AddLabels(InputIterator f, InputIterator l) 
{ 
	m_Labels.insert(m_Labels.end(), f, l);
	UpdatePartialMatchIndex();
}

template <class InputIterator>
void AssetLabels::RemoveLabels(InputIterator f, InputIterator l)
{
	std::set<std::string,PathCompareFunctor> toBeErased = std::set<std::string,PathCompareFunctor>(f,l);
	 
	for ( std::vector<UnityStr>::iterator i = m_Labels.begin(); i != m_Labels.end();  ) {
		std::vector<UnityStr>::iterator current=i++;
		if ( toBeErased.find(*current) != toBeErased.end() ) 
			m_Labels.erase(current);
	}
	UpdatePartialMatchIndex();
}


template <class InputIterator>
void AssetLabels::SetLabels(InputIterator f, InputIterator l) 
{ 
	ClearLabels();
	AddLabels(f, l); 
}

template<class TransferFunction>
void AssetLabels::Transfer (TransferFunction& transfer)
{
	TRANSFER(m_Labels);
	if( transfer.IsReading() ) {
		UpdatePartialMatchIndex();
	}
}



#endif
