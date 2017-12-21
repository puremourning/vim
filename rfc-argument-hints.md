# TL;DR

This RFC proposes introducing a second popup menu in insert mode to display
method argument hints, current parameter, etc. similar to a number of IDEs and
editors. The proposal is to allow scripts to control this (such as on insert of
`(` and `)` characters) and for it to be non-interractive and not to interfere
with insert-mode completion.

The purpose of the RFC is to guage the appetite from Bram and the community for
such a feature, and to discuss the design/functional behaviours prior to
committing to an implementation/etc.

# Motivation

There are a number of Vim plugins which attempt to provide some IDE-like
features using Vim's completion system, in combination with omnifuncs and
insert-mode completion that is built-in. Examples:

- YouCompleteMe - https://github.com/Valloric/YouCompleteMe
- neocomplete, deoplete, neocomplcache, and other friends
- eclim?
- any number of hundreds of omnifuncs
- Microsoft language server client?
- Various other "async complete" plugins
- possibly any number of other completion plugins, including implementations
  specific to particular languages (e.g. using omnifunc), or using l

In their simplest form, these provide a list of identifiers which are
semantically valid to be inserted at the current cursor position (or the result
of the equivalent of omnifunc's 'findstart' request), using the insert-mode
completion APIs provided by Vim.

One of the most commonly requested features for YouCompleteMe is the ability to
display the valid arguments for function calls, as well as semantically correct
identifiers. This feature is known as "argument hints", "parameter
completions", "signature help" or suchlike. 
[Microsoft's Language Server Protocol][lsp] refers to them as "Signature Help".

# Illustrative example

For example, the user types something like the following, with the cursor on
`^`:

```c

typedef struct pum_T 
{
  int pum_item_count;
  int pum_selected;
  pumitem_T* items[];
} pum;

void pum_display_item( pum_T* pum,
                       char_u* message,
                       pumitem_T* item_list );

void pum_display_item( pum_T* pum,
                       int selected_item,
                       pumitem_T* item );

void main(void)
{
  pum_T pum = init_pum();
  pum_display_item( pum, pum.^
}

```

Current completion systems (or omni-completion, invoked with `<C-x><C-o>`) would
typically present a popup menu with the valid options from the definition,
such as:

- `int pum_selected`
- `int pum_item_count`

Methods often have many more arguments than this toy example, and
the declaration is not likely visible when it is used. Instead, many IDEs and
other editors provide a second "popup" when the user types the `(`. Typically,
this is _above_ rather than below the current line and contains the list of
overloads of the fucntion and their arguments, often highlighting (one way or
another) the "current" argument being entered.

For example, it might look like this:

    |------------------------------------------------------------------------|
    | pum_display_item( pum_T* pum, **char_u *message**, pumitem_T* item )   |
    | pum_display_item( pum_T* pum, **int selected_item**, pumitem_T* item ) |
    |------------------------------------------------------------------------|
    pum_display_item( pum, pum.^
                               |---------------------------|
                               | pum_selected         int  |
                               | int pum_item_count   int  |
                               |---------------------------|

# Known existing attempts in Vim

- **The preview window.**
  By using the extra data in the completion menu item structure, we can show
  some static information in the preview window. The drawbacks are that the
  preview window has to always be visible (taking up valuable screen real
  estate), or it pops in and out of visibility, causing the current row to
  shift disconcertingly on the screen. Requires `preview` in the `completeopt`
  and other autocmds etc. to show/hide it. Cursor moving into and out of
  the preview window triggers `BufEnter` autocoms, which causes meaningful lag
  when the completion menu is visible (OK, this could be blamed mostly on the
  plugins using `BufEnter` autocommand, but you get the picture).

- **Oblitum/YouCompleteMe.**
  This is an approach using the standard PUM 100% with no hacks. The drawback is
  that only the current argument is displayed, and mutually exclusively with
  suggestions for the current identifier.

- a YouCompleteMe test which also uses the command line (user experience is not
  good using the command line, as the text can be clobbered by other things,
  timers, etc.)
  
- **jedi-vim.**
  A python omnifunc plugin uses conceal highlighting to simulate a second menu.
  The conceal apprach seems to involve actually changing the buffer contents and
  relying on special highlight groups to make the _appearance_ of a second
  "menu".  Ingenious, perhaps, but probably not fun to implement or maintain.
  With no disrespect to the author, it seems like a hack.

There are probably others; I just listed the ones I researched.

# Proposal

Vim currently supports a single popup menu, which is used for:

- insert-mode completion
- balloons (a more recent addition, only in the terminal)

In the GUI, balloons are implemented as a popover window via a GUI-specific
mechanism.

The proposal is to allow 2 (mostly) independent popup menus to be displayed
insert mode:

- the interractive completion menu, controlled without changes (via CTRL-X mode)
- a second menu not overlapping the completion menu controlled only by scripts.

For the purposes of this RFC (and brevity), I will refer to the menus as:

- the completion menu, and
- the hint menu

respectively.

## Display

Priority in terms of display is always given to the completion menu, as that is
the one the user interracts with. The completion menu prefers to draw below the
cursor (if there is room), above otherwise.

The hint menu prefers (for example):
 - if there is no completion menu:
    - if there is room above, above the cursor line
    - otherwise if there is blow, below the cursor
    - otherwise not at all
 - if there is a completion menu, and it wants to be below the cursor:
    - if there is room above, above the cursor line
    - otherwise, not at all
 - if there is a completion menu, and it wants to be above the cursor:
    - if there is room below, below the cursor line
    - otherwise, not at all

Both the completion menu and the hint menu are cleared when a balloon is
displayed.

## Content

Discussion point: This is open for debate and I would love to hear thoughts on
it. I see there are a couple of options here:

1. **"Pass-through"**.  The contents of the hint menu are at the discretion of
   the script. Borrowing from the approach taken for balloons, the menu supports
   a list of strings, but does not perform any additional formatting (such as
   spacing/splitting).

2. **"Call a spoon a spoon"**. The purpose of the feature is argument hints, so
   we make the API specific to that. Anything else is YAGNI.
   e.g. The API is passed a list of signatures, including the active signature
   and argument. Vim then handles the display and highlighting, allowing the
   script interface to be relatively simple, while restricting (perhaps) the
   amount of innovation.

# Implementation

One could argue that parameter hints are a special case of balloons. The special
case is that they are visible at the same time as completions in insert mode.
However, I'm not totally convinced that's the right approach. In particular,
balloons are very different in the terminal and GUI, and I feel like this feels
more similar to insert-mode completion than any existing expected use case for
balloons.

So instead, the proposal is to actualy just add (yet) another popupmenu, whose
data is independent of the existing popopmenu and balloon data.

The general idea would be to re-use most (if not all) of the existing popupmenu
code, by factoring its data out and passing an instance of "a" pum to most
functions. A vimscript API controls the hint pum, and the API for completion is
unchanged.

Here's a (very) brief idea of how I _think_ it could be done:

- Phase 1, refactor so that pum data isn't quite-so global
  - store all pum-related state in a struct pum_T
  - declare a single instance of this  called `compl_pum`
  - use `compl_pum` for completion and balloons exactly as it is used today
  - No functional change.

- Phase 2, add the second pum 
  - create a vimscript API that allows a script to display the hint pum in
    insert mode
  - provide experrimental support for:
    - hint_pum_set( {arg} )  = set the contents of the hint menu
       - arg contents TBD, but a list of strings, maybe or something else...
    - hint_pum_show( column ) = enable display of the hint menu
    - hint_pum_hide()         = disable display of the hint menu
    - hint_pum_visible()      = return whether hint menu is displayed
  - hint the hint pum when exiting insert mode (and probably a load of other
    things i haven't thought of)

- Phase 3, allow hint menu to highlight the current argument
  - Details sketchy, as they depend on some of the previous discussion points.
  - One option is to embed something, perhaps using magic string markers in the
    menu input strings, to "bold" some section.
  - Another option might be to somehow use new specific highlighting groups,
    or something along those lines, combined with knowledge about which argument
    is selected.

# Comments

I'd welcome comments on anything including:

- The welcomeness/appetite for such functionality
- The generality of the proposal (or otherwise)
- The overall approach, related ideas/works/etc.
- Niggly bikeshedding; the more the merrirer

Of course nothing is set in stone at this stage, but I did write a proof of
concept about 12 months ago as part of the research into the idea.

Some demos are attached to the gist as comments. I can point to the code in
Vim and YouCompleteMe, but it is very hacky and exploratorys, so it would be
completely re-engineered before any PR/patch is sent.

[lsp]: https://github.com/Microsoft/language-server-protocol/blob/master/protocol.md#textDocument_signatureHelp
