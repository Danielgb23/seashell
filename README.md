# Seashell

Seashell is a small experimental terminal-based Markdown reader/personal wiki written in C using `ncurses` and [MD4C](https://github.com/mity/md4c).
It is  meant as a personal project to explore text rendering, parsing, and terminal interaction on Linux.
Perhaps I evolve it into a sort of shell/envirnoment rendered from markdown files to execute lua scripts and explore rendering images 
in terminal emulators like kitty.

<img width="539" height="285" alt="image" src="https://github.com/user-attachments/assets/ecc328c3-b180-426f-862b-5ae4908dd968" />



## Features

* Parses Markdown using **MD4C**.
* Renders styled text in the terminal with **ncurses**:

  * Bold, italic, underline
  * Headers with colors
  * Inline code blocks
  * Links with highlighting
* Basic mouse support, clicking moves the cursor, clicking links enters them.
* Simple scrolling for larger documents, the app can scroll large folded lines.
* Internal representation (IR) of tokens to manage formatting.
* **Wiki mode**: Markdown links can point to other local files, allowing you to build a simple personal wiki.

## Building

Seashell depends on:

* `ncurses`
* `md4c` (already included in the code for now)
* `cmake` (for building)


Then clone and build:

```sh
git clone https://github.com/Danielgb23/seashell.git
cd seashell
mkdir build && cd build
cmake ..
make
```

The executable will be available as `Seashell` inside the `build` directory.

## Usage

Run Seashell by passing a Markdown file as an argument:

```sh
./seashell ../README.md
```
In the folder tests there are several test files. To link to another file you have to use a `file://` prefix.

### Controls

* **Keyboard / mouse**: scroll through the document.
* **Links**: hover over a link with the cursor to highlight it.
* **Wiki navigation**: pressing *Enter* on a link  to another file or clicking it will open it (if the file exists locally).

## Status

This is still a work-in-progress. 

## License

GNU 3.0 License. See [LICENSE](LICENSE) for details.
