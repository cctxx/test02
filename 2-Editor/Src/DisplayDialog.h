#pragma once

// Displays a modal dialog.
// okButton is default, cancelButton can be empty (not displayed in that case)
// Returns true if the ok button was clicked, false if the cancel button was clicked
bool DisplayDialog (const std::string& title, const std::string& content, const std::string& okButton, const std::string& cancelButton = std::string ());

// Displays a modal dialog with three buttons.
// Returns 0, 1 or 2 for okButton, secondary, third.
int DisplayDialogComplex (const std::string& title, const std::string& content, const std::string& okButton, const std::string& secondary, const std::string& third);
