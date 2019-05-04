function CanTestTermCodes()
  " This only works for Unix in a terminal
  if has('gui_running') || !has('unix')
    return v:false
  endif

  return v:true
endfunction

if !CanTestTermCodes()
  finish
endif

" xterm2 and sgr always work, urxvt is optional.
let g:ttymouse_values = ['xterm2', 'sgr']
if has('mouse_urxvt')
  call add(g:ttymouse_values, 'urxvt')
endif

" dec doesn't support all the functionality
if has('mouse_dec')
  let g:ttymouse_dec = ['dec']
else
  let g:ttymouse_dec = []
endif

" netterm only supports left click
if has('mouse_netterm')
  let g:ttymouse_netterm = ['netterm']
else
  let g:ttymouse_netterm = []
endif

" Helper function to emit a terminal escape code.
func TerminalEscapeCode(code, row, col, m)
  if &ttymouse ==# 'xterm2'
    " need to use byte encoding here.
    let str = list2str([a:code + 0x20, a:col + 0x20, a:row + 0x20])
    if has('iconv')
      let bytes = iconv(str, 'utf-8', 'latin1')
    else
      " Hopefully the numbers are not too big.
      let bytes = str
    endif
    call feedkeys("\<Esc>[M" .. bytes, 'Lx!')
  elseif &ttymouse ==# 'sgr'
    call feedkeys(printf("\<Esc>[<%d;%d;%d%s", a:code, a:col, a:row, a:m), 'Lx!')
  elseif &ttymouse ==# 'urxvt'
    call feedkeys(printf("\<Esc>[%d;%d;%dM", a:code + 0x20, a:col, a:row), 'Lx!')
  endif
endfunc

func DecEscapeCode(code, down, row, col)
    call feedkeys(printf("\<Esc>[%d;%d;%d;%d&w", a:code, a:down, a:row, a:col), 'Lx!')
endfunc

func NettermEscapeCode(row, col)
    call feedkeys(printf("\<Esc>}%d,%d\r", a:row, a:col), 'Lx!')
endfunc

func MouseLeftClick(row, col)
  if &ttymouse ==# 'dec'
    call DecEscapeCode(2, 4, a:row, a:col)
  elseif &ttymouse ==# 'netterm'
    call NettermEscapeCode(a:row, a:col)
  else
    call TerminalEscapeCode(0, a:row, a:col, 'M')
  endif
endfunc

func MouseMiddleClick(row, col)
  if &ttymouse ==# 'dec'
    call DecEscapeCode(4, 2, a:row, a:col)
  else
    call TerminalEscapeCode(1, a:row, a:col, 'M')
  endif
endfunc

func MouseCtrlLeftClick(row, col)
  let ctrl = 0x10
  call TerminalEscapeCode(0 + ctrl, a:row, a:col, 'M')
endfunc

func MouseCtrlRightClick(row, col)
  let ctrl = 0x10
  call TerminalEscapeCode(2 + ctrl, a:row, a:col, 'M')
endfunc

func MouseLeftRelease(row, col)
  if &ttymouse ==# 'dec'
    call DecEscapeCode(3, 0, a:row, a:col)
  elseif &ttymouse ==# 'netterm'
    " send nothing
  else
    call TerminalEscapeCode(3, a:row, a:col, 'm')
  endif
endfunc

func MouseMiddleRelease(row, col)
  if &ttymouse ==# 'dec'
    call DecEscapeCode(5, 0, a:row, a:col)
  else
    call TerminalEscapeCode(3, a:row, a:col, 'm')
  endif
endfunc

func MouseRightRelease(row, col)
  call TerminalEscapeCode(3, a:row, a:col, 'm')
endfunc

func MouseLeftDrag(row, col)
  if &ttymouse ==# 'dec'
    call DecEscapeCode(1, 4, a:row, a:col)
  else
    call TerminalEscapeCode(0x20, a:row, a:col, 'M')
  endif
endfunc

func MouseWheelUp(row, col)
  call TerminalEscapeCode(0x40, a:row, a:col, 'M')
endfunc

func MouseWheelDown(row, col)
  call TerminalEscapeCode(0x41, a:row, a:col, 'M')
endfunc


