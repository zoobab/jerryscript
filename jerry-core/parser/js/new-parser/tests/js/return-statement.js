/* This test contains throw statement parser tests. */

next_statement;

//function return_undefined() {
    return;
    { return }
//}

next_statement;

//function return_expression() {
    { return exp }
//}

next_statement;

//function return_undefined_and_dead_code() {
    return
    exp
//}

next_statement;
