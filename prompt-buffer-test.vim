set buftype=prompt

call prompt_setprompt( bufnr(), 'Enter some text: ' )

call job_start( [ 'uuencode', '/dev/urandom', '/dev/stdout' ], #{
      \ out_io: 'buffer',
      \ out_name: ''
      \ } )
