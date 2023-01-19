# MX Text Edtior #

Mx is a minimalistic text editor with emacs-like keybindings. It depends only on the GNU C core libraries. This is a toy text editor so better backup your files before trying it out. It is licensed under the [GPLv3](https://www.gnu.org/licenses/gpl.html). Please refer to the LICENSE file for detailed information.

### Compiling and running ###
```
git clone https://github.com/aroess/mx-editor
cd mx-editor
make
./mx my_file
# If you are brave
# cp mx /usr/bin
```

### Screenshot ###
![screenshot](https://raw.githubusercontent.com/aroess/mx-editor/master/screenshot.png "Mx editing its own source code")

### Key bindings ###

| Command | Description |
| --- | --- |
| ```C-x C-c``` | Exit mx |
| ```C-x C-s``` | Save document |
| ```C-x =``` | Print info on cursor position |
|``` C-g``` | Exit minibuffer |
| ```C-f``` | Forward char |
|``` C-b``` | Backward char |
|``` M-f``` | Forward word |
|``` M-b``` | Backward word |
|``` M-d``` | Delete forward word |
|``` C-a``` | Move to beginning of line |
|``` C-e``` | Move to end of line |
|``` C-n``` | Move to next line |
|``` C-p``` | Move to previous line |
|``` C-s``` | Search forward |
|``` M-g``` | Goto line |
|``` C-v``` | Page down |
|``` M-v``` | Page up |
|``` M-,``` | Move to beginning of document |
|``` M-.``` | Move to end of document |
|``` C-l``` | Center cursor |
|``` C-k``` | Kill to end of line |
|``` C-u``` | Kill to beginning of line |
|``` C-d``` | Delete char forward |
|``` RET``` | Insert newline |
|``` BACKSPACE``` | Delete char backwards / delete line |
| otherwise | insert self |

### TODO ###
- "Save as" function
- "Undo" function
- ~~incremental~~ ~~forward~~/backward search
