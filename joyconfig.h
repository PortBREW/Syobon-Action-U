// Nintendo Wii U controller configuration for Syobon Action U.
//
// The devkitPro SDL2 Wii U joystick driver exposes the GamePad, Wii U
// Pro Controller, Classic Controller, Wii Remote and Nunchuk with a
// consistent button layout, so a single mapping covers all of them.
// See DxLib.cpp (PollGamepad) for the actual handling.
//
// Wii U control     Game action
// ----------------  -----------------------------------------
// D-pad Left/Right  Move left / right
// Left stick        Move (analogue)
// A / B             Jump
// START (+)         Return to the title screen (menu)
// Touch screen      Confirm / start (acts like Z)

//Axis numbers, usually 0 and 1, although I have seen others
#define JOYSTICK_XAXIS 0
#define JOYSTICK_YAXIS 1

//The button number that will be used for jumping (A on Wii U)
#define JOYSTICK_JUMP 0
