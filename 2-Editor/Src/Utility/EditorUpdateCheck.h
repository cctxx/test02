#ifndef EDITOR_UPDATE_CHECK_H
#define EDITOR_UPDATE_CHECK_H

bool IsTimeToRunUpdateCheck();

enum ShowUpdateWindow
{
	kDoNotShow = 0, 
	kShowIfNewerVersionExists, 
	kShowAlways
};
/// Checks for a newer version of the Editor
void EditorUpdateCheck(ShowUpdateWindow showUpdateWindow, bool editorstartup);

#endif
