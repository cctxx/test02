// NOTE: the file _has to_ be UTF8 with BOM marker, otherwise
// non-English characters in credits won't be displayed correctly!

using UnityEngine;
using System;
using System.Collections;
using UnityEditor;
using UnityEditorInternal;

namespace UnityEditor {

internal class AboutWindow : EditorWindow {
	static void ShowAboutWindow () {
        AboutWindow w = EditorWindow.GetWindowWithRect<AboutWindow>(new Rect(100, 100, 570, 340), true, "About Unity");
		w.position = new Rect(100,100,570,340);
		w.m_Parent.window.m_DontSaveToLayout = true;
	}

	private static GUIContent s_MonoLogo, s_AgeiaLogo, s_UnityLogo, s_Header;
	//private Vector2 m_CreditsPos = Vector2.zero;
	
	private static void LoadLogos()
	{
		if( s_MonoLogo != null )
			return;
		s_MonoLogo = EditorGUIUtility.IconContent ("MonoLogo");
		s_AgeiaLogo = EditorGUIUtility.IconContent ("AgeiaLogo");
		s_UnityLogo = EditorGUIUtility.IconContent ("UnityLogo");
		s_Header = EditorGUIUtility.IconContent ("AboutWindow.MainHeader");
	}

	float m_TextYPos = 120;
		
	public void OnInspectorUpdate () {
		m_TextYPos -= 1f;
		if (m_TextYPos < -950)
			m_TextYPos = 120;
		Repaint ();
	}
		
	bool m_ShowDetailedVersion = false;
	public void OnGUI() {
			
		LoadLogos();
			
		GUILayout.BeginHorizontal();
		GUILayout.Space (5);
		GUILayout.Label(s_UnityLogo, GUIStyle.none, GUILayout.ExpandWidth (false));
		GUILayout.BeginVertical ();
		GUILayout.FlexibleSpace ();
		GUILayout.Label(s_Header, GUIStyle.none);
		
		ListenForSecretCodes ();
		
		m_ShowDetailedVersion |= Event.current.alt;
		if ( m_ShowDetailedVersion ) 
		{
			int t = InternalEditorUtility.GetUnityVersionDate ();
			DateTime dt = new DateTime(1970, 1, 1, 0, 0, 0, 0);
			string url = InternalEditorUtility.GetUnityBuildBranchUrl();
			string urlString = "";
			if ( url.Length > 0 ) 
			{
				urlString = "Branch: " + url;
			}
			EditorGUILayout.SelectableLabel("Version " + InternalEditorUtility.GetFullUnityVersion() + "\n" + String.Format("{0:r}", dt.AddSeconds(t)) + "\n" + urlString, GUILayout.Width(400), GUILayout.Height(40));
		}
		else
		{
			GUILayout.Label("Version " + Application.unityVersion);
		}

		if (Event.current.type == EventType.ValidateCommand)
			return;

		GUILayout.Space (8);
		GUILayout.EndVertical ();
		GUILayout.EndHorizontal();
		
		GUILayout.FlexibleSpace ();		

		GUI.BeginGroup (GUILayoutUtility.GetRect (10, 120));

		GUI.Label(new Rect (5, m_TextYPos, position.width-10, 950),
				@"Adam Buckner, Adam Dials, Adam Gutterman, Adriano Carlos Verona, Agnieszka Loza, Akouvi Ahoomey, Aleksander Grigorenko, Aleksandr Dubinskiy, Alex McCredie, Alex Thibodeau, Alexandra Mariner, Alexey Abramychev, Alexey Orlov, " +
				@"Allen Foo, Amir Ebrahimi, Andrew L. Tang, Andreas Hansen, Andreia Gaita, Andrey Shvets, Andrius Keidonas, Andrius Kuznecovas, Andy Brammall, Andy Stark, Andy Touch, Ann Byrdal Michaelsen, Antony Douglas, Aras Pranckevičius, " +
				@"Artiom Koshelev, Aurimas Cernius, Aurimas Gasiulis, Aurore Dimopoulos, Aya Yoshizaki, Bas Smit, Beau Folsom, Bee Ling Chua, Ben Pitt, Ben Stoneman, Benjamin Quorning, Beth Thomas, Bjørn Göttler, Bo S. Mortensen, Bobo Bo, Bobby Billingsley, " +
				@"Brad Robbel-Forrest, Brett Bibby, Brian E. Wilson, Brian Hu, Biran Mccoy, Brett Seyler, Caitlyn Meeks, Carl Callewaert, Cathy Yates, Cecilie Mosfeldt, Charles Hinshaw, Charlotte Kaas Larsen, Chida Chaemchaeng, Chris Migleo, " +
				@"Christian Bell Bastlund, Christopher Pope, Christopher Owen Hamilton, Claude Comeau, Claus Petersen, Corey Johnson, Craig Matthew Blum, Dan Adams, Dana Ramnarine, Daniel Bratcher, Daniel Collin, Daniel Tan, Darren Williams, " +
				@"Dave Shorter, Davey Jackson, David Berkan, David Helgason, David Liew, David Llewelyn, David Oh, David Rogers, Denis Simuntis, Dennis DeRyke, " +
				@"Dmitry Onishchenko, Dmitry Shtainer, Dmytro Mindra, Dominykas Kiauleikis, Donaira Tamulynaitė, Edward Epstein, Edward Yablonsky, Ekaterina Kalygina, Elena Savinova, Elizabeth Rankich, Ellen Liew, Elliot Solomon, Elvis Alistar, Emil Johansen, Emily Emanuel, " +
				@"Erik Hemming, Erik Juhl, Erin Baker, Erland Körner, Esben Ask Meincke, Ethan Vosburgh, Ezra Nuite, Fini Faria Alring, Frank Jonsson, Fredrik von Renteln, Freyr Helgason, Gabriele Farina, Graham Dunnett, Gukhwan Ji, " +
				@"Hanna Yi, Heidi Therkildsen, Heine Meineche Fusager, Henrik Nielsen, Hiroki Omae, Hólmfríður Eygló Arndal Gunnarsdóttir, HoMin Lee, Hugh Longworth, Hui Xiumo, Hwan-hee Kim, Ian Dunlap, Ian Dundore, Ignas Ziberkas, " +
				@"Ilya Komendantov, Ilya Turshatov, Isabelle Jacquinot, Jack Kieran Paine, Jakob Hunsballe, Jakub ""Kuba"" Cupisz, James Cho, James Bouckley, Jan Marguc, Jason Parks, Jay Clei Garcia dos Santos, Jeanne Dieu, Jed Ritchey, Jeff Aydelotte, Jens Fursund, " +
				@"Jens Andersen, Jesper Mortensen, Jessica Qian, Jessika Jackson, Jinho Mang, Jiwon Yu, Jonathan Chambers, Joachim Ante, Joana Koyte, JoAnna Matthisen, Joe Robins, Joe Santos, Joen Joensen, John Goodale, John Edgar Congote Calle, " +
				@"Jonas Echterhoff, Jonas Christian Drewsen, Jonas Meyer, Jonas Törnquist, Jonathan Chambers, Jonathan Oster Dano, Jookyung Hyun, Joseph Walters, Juan Sebastian Muñoz, Juha Kiili, Jukka Arvo, " +
				@"Julie Eickhof, Julius Miknevičius, Julius Trinkunas, Junghwa (Elisa) Choi, Jussi Venho, Justin Kruger, Justinas Daugmaudis, Kamio Chambless, Karen Riskær Jørgensen, Karsten Nielsen, " +
				@"Kaspar Daugaard, Kasper Amstrup Andersen, Katrine Hegnsborg Bruun, Katherine Overmeyer, Kazimieras Semaška, Keijiro Takahashi, Keli Hlöðversson, Kelly Sandvig, Kelvin Lo, Kevin Ashman, " +
				@"Kevin Robertson, Kia Skouw Christensen, Kiersten Petesch, Kim Moon-soo, Kim Steen Riber, Kimberly Bailey, Kimberly Verde, Kimi Wang, Kjartan Olafsson, Kornel Mikes, " +
				@"Kristjan B. Halfdanarson, Kristian Mandrup, Lars ""Kroll"" Kristensen, Lars Runov, Lars Mølgård Nielsen, Lárus Ólafsson, Lasse Järvensivu, Lasse Makholm, Leo Yaik Shiin Paan, Leon Jun, Leonardo Carneiro, Levi Bard, " +
				@"Liang Zhao, Lucas Meijer, Loreta Balčiūnaitė, Louise Skaarup, Madelaine Fouts, Mads Kiilerich, Mads Kjellerup, Mads Nyholm, Maj-Brit Jo Arnested, " +
				@"Makotoh Itoh, Mantas Puida, Marco Alvarado, Marcus Lim, Marek Turski, Maria Marcano, Marina Øster, Mark Harkness, Mark T. Morrison, Martin Eberhardt, Martin Gjaldbæk, " +
				@"Martin Nielsen, Martin Sternevald, Martin Stjernholk Vium, Martin Troels Eberhardt, Marvin Kharrazi, Massimiliano Mantione, Massimo Caterino, Matt Reynolds, Matthew Fini, Melvyn May, Michael Edmonds, Michael Krarup Nielsen, " +
				@"Mikko Strandborg, Mikkel ""Frecle"" Fredborg, Mircea Marghidanu, Monika Madrid, Morrissey Williams, Morten Sommer, Mykhailo Lyashenko, Na'Tosha Bard, Navin Kumar Chaudhary, " +
				@"Nevin Eronde, Ngozi Watts, Nicola Evans, Nicholas Francis, Nick Jovic, Nicolaj Schweitz, Nobuyuki Kobayashi, Ole Ciliox, " +
				@"Oleg Pridiuk, Olly Nicholson, Oren Tversky, Paul Tham, Patrick Williamson, Paulius Liekis, Peden Fitzhugh, Pernille Hansen, Peter Ejby Dahl Jensen, Peter Kuhn, " +
				@"Peter Schmitz, Petri Nordlund, Philip Cosgrave, Pierre Paul Giroux, Pyry Haulos, Qing Feng, Ralph Hauwert, Randy Spong, Rasmus Møller Selsmark, Rasmus ""Razu"" Boserup, Rebekah Tay Xiao Ping, Renaldas ""ReJ"" Zioma, René Damm, " +
				@"Ricardo Arango, Rickard Andersson, Richard Sykes, Rita Turkowski, Roald Høyer-Hansen, Rob Fairchild, Robert Brackenridge, Robert Cassidy, Robert Cupisz, Robert Lanciault, " +
				@"Robert Oates, Rodrigo B. de Oliveira, Rodrigo Lopez-carrillo, Roman Glushchenko, " +
				@"Ronnie (Seyoon) Jang, Rune Skovbo Johansen, Ruslan Grigoryev, Ryan N. Burke, Rytis Bieliūnas, Sakari Pitkänen, Samantha Kalman, Sara Cannon, Sara Wallman, Scott Flynn, " +
				@"Sean Baggaley, Sergej Kravcenko, Sergio Gomez, Shanti Zachariah, Shawn White, Shinobu Toyoda, Silvia Rasheva, Silviu Ionescu, Simon Holm Nielsen, Sin Jin Chia, Skjalm Arrøe, Sonny Myette, Steen Lund, " +
				@"Stefan Sandberg, Stefan Schubert, Steffen Toksvig, Sten Selander, Stephanie Chen, Stine Munkesø Kjærbøll, Suhail Dutta, Susan Anderson, Søren Christiansen, Tatsuhiko Yamamura, " +
				@"Tec Liu, Terry Hendrix II, Thomas Bentzen, Thomas Cho, Thomas Fejerskov Klindt, Thomas Golzen, Thomas Grové, Thomas Hagen Johansen, Thomas Harkjær Petersen, Thomas Kristiansen, Thomas Svaerke, Timothy Cooper, Todd Hooper, Todd Rutherford, Tom Higgins, " +
				@"Tomas Dirvanauskas, Tomas Jakubauskas, Tomasz Paszek, Tony Garcia, Torben Jeppesen, Toshiyuki Mori, Tracy Erickson, Tricia Gray, Ugnius Dovidauskas, Vadim Kuzmenko, Valdemar Bučilko, Veli-Pekka Kokkonen, Venkatesh Subramaniam Pillai, Vibe Herlitschek, " +
				@"Victor Suhak, Vilius Prakapas, Vilmantas Balasevičius, Vincent Zhang, Vitaly Veligursky, Vytautas Šaltenis, Wayne Johnson, Wnedy Tan Woon Li, Wenqi Zhang, Weronika Weglinska, " +
				@"Will Goldstone, William Hugo Yang, William Nilsson, William ""Pete"" Moss, Xiao Ling Yao, Xin Zhang, Yan Drugalya, Yelena Danziger, Yohei Tanaka, Yunkyu Choi, Yoo Kyoung Lee, " +
				@"Yuan Kuan Seng, Younghee Cho, Zachary Zadell, Zbignev Monkevic, Zhenping Guo", EditorStyles.wordWrappedLabel);
		GUI.Label(new Rect(5, m_TextYPos + 950, position.width - 10, 950), "Thanks to Forest 'Yoggy' Johnson, Graham McAllister, David Janik-Jones, Raimund Schumacher, Alan J. Dickins and Emil 'Humus' Persson", EditorStyles.wordWrappedMiniLabel);
		GUI.EndGroup ();
		
		GUILayout.FlexibleSpace ();		

		GUILayout.BeginHorizontal ();
			GUILayout.Label (s_MonoLogo);
			GUILayout.Label ("Scripting powered by The Mono Project.\n\n(c) 2011 Novell, Inc.", "MiniLabel", GUILayout.Width (200));
			GUILayout.Label (s_AgeiaLogo);
			GUILayout.Label ("Physics powered by PhysX.\n\n(c) 2011 NVIDIA Corporation.", "MiniLabel", GUILayout.Width (200));
		GUILayout.EndHorizontal ();
		GUILayout.FlexibleSpace ();
		GUILayout.BeginHorizontal ();
			GUILayout.Space (5);
			GUILayout.BeginVertical ();
				GUILayout.FlexibleSpace ();
				GUILayout.Label("\n" + InternalEditorUtility.GetUnityCopyright(), "MiniLabel");
			GUILayout.EndVertical ();
			GUILayout.Space (10) ;
			GUILayout.FlexibleSpace ();
			GUILayout.BeginVertical ();
				GUILayout.FlexibleSpace ();
				GUILayout.Label(InternalEditorUtility.GetLicenseInfo(), "AboutWindowLicenseLabel");
			GUILayout.EndVertical ();
			GUILayout.Space (5);
		GUILayout.EndHorizontal ();
			
		GUILayout.Space(5);
	}
	
	private int m_InternalCodeProgress;
	private void ListenForSecretCodes ()
	{
		if (Event.current.type != EventType.KeyDown || (int)Event.current.character == 0)
			return;
		
		if (SecretCodeHasBeenTyped ("internal", ref m_InternalCodeProgress))
		{
			bool enabled = !EditorPrefs.GetBool ("InternalMode", false);
			EditorPrefs.SetBool ("InternalMode", enabled);
			ShowNotification (new GUIContent ("Internal Mode " + (enabled ? "On" : "Off")));
			InternalEditorUtility.RequestScriptReload ();
		}
	}
	
	private bool SecretCodeHasBeenTyped (string code, ref int characterProgress)
	{
		if (characterProgress < 0 || characterProgress >= code.Length || code[characterProgress] != Event.current.character)
			characterProgress = 0;
		
		// Don't use else here. Even if key was mismatch, it should still be recognized as first key of sequence if it matches.
		if (code[characterProgress] == Event.current.character)
		{
			characterProgress++;
			
			if (characterProgress >= code.Length)
			{
				characterProgress = 0;
				return true;
			}
		}
		return false;
	}
}

} // namespace
