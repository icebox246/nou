" syntax region NOUString start=/\v"/ skip=/\v\\./ end=/\v"/
" highligh link NOUString String

" syntax region NOUChar start=/\v'/ skip=/\v\\./ end=/\v'/
" highligh link NOUChar Character

syn match NOUNumber /\<[0-9]\+\([ui][0-9]\+\)\?\>/
highligh link NOUNumber Number

syn keyword NOUKeyword export extern fn return if else
highligh link NOUKeyword Keyword

syn keyword NOUType u8 i32 bool
highligh link NOUType Type

syn keyword NOUBoolean true false 
highligh link NOUBoolean Boolean

" syn keyword NOUConstant null
" highligh link NOUConstant Constant

syn match NOUFunction /\<[A-Za-z_][A-Za-z0-9_]*\(\s*(\)\@=/
syn match NOUFunction /\<[A-Za-z_][A-Za-z0-9_]*\(\s*:=\s*fn\>\)\@=/
syn match NOUFunction /\(\<export\s*\)\@<=\<[A-Za-z_][A-Za-z0-9_]*/
highligh link NOUFunction Function

syn match NOUOperator "="
syn match NOUOperator "=="
syn match NOUOperator "+"
syn match NOUOperator "-" 
syn match NOUOperator "*"
syn match NOUOperator "/"
syn match NOUOperator "%"
syn match NOUOperator "\<and\>"
syn match NOUOperator "\<or\>"
highligh link NOUOperator Operator

syn match NOUDelimiter ":"
syn match NOUDelimiter "}"
syn match NOUDelimiter "{"
syn match NOUDelimiter "("
syn match NOUDelimiter ")"
syn match NOUDelimiter ","
syn match NOUDelimiter ";"
syn match NOUDelimiter "->"
syn match NOUDelimiter ":="
highligh link NOUDelimiter Delimiter

syntax match NOUComment "\v//.*$"
highligh link NOUComment Comment
