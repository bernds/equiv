## Equiv, a simple image viewer with nondestructive editing tools

This is a little image viewing/editing tool I built mainly for myself.
Its main goal is to be lightweight, have a simple keyboard-controlled UI
and a few persistent, nondestructive editing tools. If anyone other than
me finds it useful - great! But I'm not expecting world domination with
this.

There is no need to import images into a database, but Equiv keeps a
database of previously seen images if they had nondestructive edits.
This database identifies images by a hash of the file contents, which
means files can be freely moved in the file system outside of this
application, or hard linked into collections, without losing edits.

It takes inspiration from a few other programs that I use:
- xzgv is my go-to image viewer - I like its simplicity and minimalist
  interface. Its downsides are bad scaling and using GTK (which makes me
  not want to modify it to suit my needs).
- EOM is a little better, but also uses GTK and is less good at
  navigating the file system.
- The late great Picasa was a fantastic tool, but it is abandoned, and not
  exactly native on Linux or open source. I miss the nondestructive
  editing, but I'd rather not have Picasa.ini files everywhere

At the moment, this program is a few days old and therefore probably still
somewhat rough. It's good enough for me to replace the tools I was
previously using.

## Usage

Pass the name of an image or directory on the command line.

Equiv is mostly controlled through keyboard shortcuts.
- Space to advance in the list of images, 'b' to go back
- 'f' and 't' to show/hide the side panes.
- F5 to start a randomized slide show.
- 'z' to toggle scale mode.
- Ctrl-'c' and Ctrl-'v' to copy and paste image tweaks, where the copy
  always includes the full set, and the information to be pasted is
  controllable by check boxes in the tuning pane.

## Future plans

These may or may not happen, but it might be nice to add:
- Thumbnails support. (Use the preview support code from q5go/GAPFix)
- Cropping support
- EXIF support
- Applying edits. Use jpegtran for nondestructive transformations.
- Undo/redo would be useful
- Support collections by defining special directories and hardlinking
  images there.

## Requirements

These are the prerequisites for building and running this program:
- The Qt library for the GUI

## Compiling

On Linux, make a build subdirectory, enter it, and run
```sh
  qmake ../src/equiv.pro PREFIX=/where/you/want/to/install
```
followed by make and make install.

## License

Equiv is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Equiv is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Equiv.  If not, see <http://www.gnu.org/licenses/>.

## Credits

The Undo icon is from the Tango icon collection, licensed as public
domain.
