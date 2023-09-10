const defaultText = `// make run visible outside the app
export run;

// logs int value to output
extern log_int := fn x: i32;
// asks user for int value
extern get_int := fn default: i32 -> i32;

// this is the entry point of the app
run := fn {
    // add 1 and 2, and store the result in res
    res := i32 add(1, 2);
    log_int(res);

    // uncomment line below to see interactive example
    //interactive_doubling();
};

add := fn a: i32, b: i32 -> i32 {
    return a + b;
};

interactive_doubling := fn {
    num := i32;
    num = get_int(21);
    log_int(2 * num);
};`

/**
 * @type {(element: HTMLTextAreaElement)}
 **/
export function setupCodeEditor(element) {
    element.classList.add("code-editor");
    element.spellcheck = false;
    element.value = defaultText;

    return {
        getSourceCode() {
            return element.value;
        }
    };
}