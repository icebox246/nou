import defaultText from './example.u?raw'
import { highlightCode } from './highlight';

/**
 * @type {(element: HTMLTextAreaElement)}
 **/
export function setupCodeEditor(element) {
    element.classList.add("code-editor-container");

    const textarea = document.createElement("textarea");
    textarea.classList.add("code-editor-textarea")
    textarea.spellcheck = false;
    textarea.wrap = "off";
    textarea.value = defaultText;

    textarea.addEventListener("keydown", (e) => {
        if (e.key === "Tab") {
            e.preventDefault();
            const start = textarea.selectionStart;
            const end = textarea.selectionEnd;
            const indent = "  "
            if (!e.shiftKey) {

                textarea.value = textarea.value.substring(0, start)
                    + indent + textarea.value.substring(end);

                textarea.selectionStart = start + indent.length;
                textarea.selectionEnd = start + indent.length;
            } else {
                if (start < indent.length) return;
                textarea.value = textarea.value.substring(0, start - indent.length) + textarea.value.substring(end);
                textarea.selectionStart = start - indent.length;
                textarea.selectionEnd = start - indent.length;
            }
        }
        console.log(e.key)
    });
    element.appendChild(textarea);

    const highlighted = document.createElement("p");
    highlighted.classList.add("code-editor-highlighted");

    function updateHighlighedText() {
        const text = textarea.value;

        const highlighted_text = highlightCode(text);

        highlighted.innerHTML = highlighted_text;
        textarea.style.height = `${textarea.scrollHeight}px`;
        textarea.style.width = `${textarea.scrollWidth}px`;
    }

    textarea.addEventListener("input", updateHighlighedText);
    updateHighlighedText();

    element.appendChild(highlighted);

    return {
        getSourceCode() {
            return textarea.value;
        }
    };
}
