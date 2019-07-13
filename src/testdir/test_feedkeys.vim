" Test feedkeys() function.

func Test_feedkeys_x_with_empty_string()
  new
  call feedkeys("ifoo\<Esc>")
  call assert_equal('', getline('.'))
  call feedkeys('', 'x')
  call assert_equal('foo', getline('.'))

  " check it goes back to normal mode immediately.
  call feedkeys('i', 'x')
  call assert_equal('foo', getline('.'))
  quit!
endfunc

func Test_feedkeys_TextChangedI()
  let g:Test_feedkeys_TextChangedI_called = 0
  augroup Test_feedkeys_TextChangedI
    au!
    au TextChangedI * :let g:Test_feedkeys_TextChangedI_called=1
  augroup END

  enew
  call test_override( 'char_avail', 1 )
  call feedkeys( 'ithis is some text', 'xt' )
  call test_override( 'char_avail', 0 )
  call assert_equal( 'this is some text', getline( '.' ) )
  call assert_true( g:Test_feedkeys_TextChangedI_called )

  augroup Test_feedkeys_TextChangedI
    au!
  augroup END
  %bwipe!
endfunc
