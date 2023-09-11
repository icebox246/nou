import defaultText from './example.u?raw'

/**
 * @type {(element: HTMLTextAreaElement)}
 **/
export function setupCodeEditor(element) {
    element.classList.add("code-editor");
    element.spellcheck = false;
    element.wrap = "off";
    element.value = defaultText;

    return {
        getSourceCode() {
            return element.value;
        }
    };
}
