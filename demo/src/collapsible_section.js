/**
    @type {(element: HTMLElement)}
**/
export function setupCollapsibleSection(element, title, starts_collapsed) {
    element.classList.add("collapsible-section");
    if(starts_collapsed) {
        element.classList.add("collapsed")
    }
    const header = document.createElement("header");
    const title_span = document.createElement("span");
    title_span.innerText = title;
    header.appendChild(title_span);

    const line_count_span = document.createElement("span");
    line_count_span.innerText = "0 lines";
    header.appendChild(line_count_span);

    element.appendChild(header);

    const content = document.createElement("pre");
    content.innerText = "<empty>";

    element.appendChild(content);

    header.addEventListener('click', () => { element.classList.toggle("collapsed"); });

    return {
        /** @type {(text: string)} **/
        setContent(text) {
            content.innerText = text;
            const line_count = text.split("\n").length - 1;
            line_count_span.innerText = `${line_count} lines`;
        }
    }
}
