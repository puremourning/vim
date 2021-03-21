source ~/Development/vimways/a-test-to-attest-to/test/support/test_async.vim

let g:attest_async_delay = 1

function ExitInsertMode()
  call feedkeys( "\<Esc>" )
endfunction

augroup Test_Async
  au CompleteDone * call ExitInsertMode()
augroup END

call popup_create( [ 'This is some text', 'and some more' ], {} )


let i = 0
while i < 30000
  call feedkeys( "o\<C-x>\<C-u>", 'xt!' )

  let i += 1
endwhile

augroup Test_Async
  au!
augroup END

call popup_clear()

delfunc! ExitInsertMode
%bwipe!
qa
