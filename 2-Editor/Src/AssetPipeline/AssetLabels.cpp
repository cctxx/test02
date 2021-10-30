/*
 *  AssetLabels.cpp
 *  Xcode
 *
 *  Created by Hrafnkell Hlodversson on 2009-11-16.
 *  Copyright 2009 Unity Technologies ApS. All rights reserved.
 *
 */

#include "UnityPrefix.h"
#include "AssetLabels.h"

bool AssetLabels::Match(const UnityStr& label) const 
{ 
	return  m_PartialAcc.find(ToLower(label)) != UnityStr::npos; 
} 


void AssetLabels::UpdatePartialMatchIndex() 
{
	m_PartialAcc.clear();
    for (std::vector<UnityStr>::const_iterator i = m_Labels.begin (); i !=  m_Labels.end (); ++i)
	{
		m_PartialAcc.append ("#");
        m_PartialAcc.append (ToLower(*i).c_str ());
    }	
	m_PartialAcc.append ("#");
}