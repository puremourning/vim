" Test WinBar

if !has('menu')
  finish
endif

source shared.vim

func Test_add_remove_menu()
  new
  amenu 1.10 WinBar.Next :let g:did_next = 11<CR>
  amenu 1.20 WinBar.Cont :let g:did_cont = 12<CR>
  redraw

  call assert_match( 'Next', Screenline( 1 ) )
  call assert_match( 'Cont', Screenline( 1 ) )

  emenu WinBar.Next
  call assert_equal(11, g:did_next)
  emenu WinBar.Cont
  call assert_equal(12, g:did_cont)

  wincmd w
  call assert_fails('emenu WinBar.Next', 'E334')
  wincmd p

  aunmenu WinBar.Next
  aunmenu WinBar.Cont
  close
endfunc

func Test_click_in_winbar()
  new
  amenu 1.10 WinBar.Next :let g:did_next = 11<CR>
  amenu 1.20 WinBar.Cont :let g:did_cont = 12<CR>
  amenu 1.30 WinBar.Close :close<CR>
  redraw

  let save_mouse = &mouse
  set mouse=a

  " Columns of the button edges:
  " _Next_  _Cont_  _Close_
  " 2    7  10  15  18   24
  let g:did_next = 0
  let g:did_cont = 0
  for col in [1, 8, 9, 16, 17, 25, 26]
    call test_setmouse(1, col)
    call feedkeys("\<LeftMouse>", "xt")
    call assert_equal(0, g:did_next, 'col ' .. col)
    call assert_equal(0, g:did_cont, 'col ' .. col)
  endfor

  for col in range(2, 7)
    let g:did_next = 0
    call test_setmouse(1, col)
    call feedkeys("\<LeftMouse>", "xt")
    call assert_equal(11, g:did_next, 'col ' .. col)
  endfor

  for col in range(10, 15)
    let g:did_cont = 0
    call test_setmouse(1, col)
    call feedkeys("\<LeftMouse>", "xt")
    call assert_equal(12, g:did_cont, 'col ' .. col)
  endfor

  let wincount = winnr('$')
  call test_setmouse(1, 20)
  call feedkeys("\<LeftMouse>", "xt")
  call assert_equal(wincount - 1, winnr('$'))

  let &mouse = save_mouse
endfunction

source shared_termcodes.vim

if !CanTestTermCodes()
  finish
endif

function! Test_Winbar_Click_Not_Current()
  set mouse=a
  for ttymouse_val in g:ttymouse_values + g:ttymouse_dec + g:ttymouse_netterm
    exe 'edit X' .. ttymouse_val .. 'menubar'
    nnoremenu WinBar.Click :close<CR>
    redraw
    call assert_match( 'Click', Screenline( 1 ) )

    " Switch a new buffer
    botright split 'X' .. ttymouse_val .. "empty"

    let msg = ttymouse_val
    exe 'set ttymouse=' .. ttymouse_val

    " The winbar is on the first row, and the button is offset by at least one
    " column
    let row = 1
    let col = 3

    let wincount = winnr('$')
    call MouseLeftClick(row, col)
    call assert_equal( wincount - 1, winnr('$') )

    %bwipeout!
  endfor

  set mouse&
  set ttymouse&
  %bwipeout!
endfunc
