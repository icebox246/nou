/** 
  @type {(element: HTMLButtonElement)}  
**/
export function setupButton(element, callback) {
    element.classList.add("button");
    if (callback)
        element.addEventListener("click", callback);
}
