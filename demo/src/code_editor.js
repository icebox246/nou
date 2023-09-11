import defaultText from './example.u?raw'

/**
 * @type {(element: HTMLTextAreaElement)}
 **/
export function setupCodeEditor(element) {
    element.classList.add("code-editor");
    element.spellcheck = false;
    element.wrap = "off";
    element.value = defaultText;

    element.addEventListener("keydown", (e) => {
        if (e.key === "Tab") {
            e.preventDefault();
            const start = element.selectionStart;
            const end = element.selectionEnd;
            const indent = "  "
            if (!e.shiftKey) {

                element.value = element.value.substring(0, start)
                    + indent + element.value.substring(end);

                element.selectionStart = start + indent.length;
                element.selectionEnd = start + indent.length;
            } else {
                if (start < indent.length) return;
                element.value = element.value.substring(0, start - indent.length) + element.value.substring(end);
                element.selectionStart = start - indent.length;
                element.selectionEnd = start - indent.length;
            }
        }
        console.log(e.key)
    });

    return {
        getSourceCode() {
            return element.value;
        }
    };
}
