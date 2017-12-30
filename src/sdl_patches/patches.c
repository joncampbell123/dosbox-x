/* DOSBox-X deviation: hack to ignore Num/Scroll/Caps if set */
#if defined(WIN32)
unsigned char _dosbox_x_hack_ignore_toggle_keys = 0;
unsigned char _dosbox_x_hack_wm_user100_to_keyevent = 0;

void __declspec(dllexport) SDL_DOSBox_X_Hack_Set_Toggle_Key_WM_USER_Hack(unsigned char x) {
	_dosbox_x_hack_ignore_toggle_keys = x;
	_dosbox_x_hack_wm_user100_to_keyevent = x;
}
#endif