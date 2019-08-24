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

source check.vim

func Test_feedkeys_dangerous()
  CheckFeature timers

  new

  func TimerCallback( id ) closure
    call assert_equal( 'i', mode() )
    call feedkeys( "\<CR>And so is this\<ESC>" )
  endfunc

  call timer_start( 500, funcref( 'TimerCallback' ) )
  call feedkeys( "iThis is a test", "tx!" )

  call assert_equal( 'n', mode() )

  call assert_equal( [ 'This is a test',  'And so is this' ],
        \            getline( 1, 2 ) )

  delfun TimerCallback
  %bwipe!
endfunc

func Test_feedkeys_dangerous_recursive()
  CheckFeature timers

  new

  func TimerCallback2( id ) closure
    call assert_equal( 'i', mode() )
    call feedkeys( "\<CR>A third test. Oh my!\<ESC>", "t" )
  endfunc

  func TimerCallback( id ) closure
    call assert_equal( 'i', mode() )
    call timer_start( 500, funcref( 'TimerCallback2' ) )
    call feedkeys( "\<CR>And so is this", "t!" )
  endfunc

  call timer_start( 500, funcref( 'TimerCallback' ) )
  call feedkeys( "iThis is a test", "tx!" )

  call assert_equal( 'n', mode() )

  call assert_equal( [ 'This is a test',
        \               'And so is this',
        \               'A third test. Oh my!' ],
        \            getline( 1, 2 ) )

  delfun TimerCallback
  delfun TimerCallback2
  %bwipe!
endfunc
