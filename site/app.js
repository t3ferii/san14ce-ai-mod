"use strict";
const assetRoot = "https://raw.githubusercontent.com/t3ferii/san14ce-ai-mod/main/readme_screenshot/";
document.querySelectorAll("[data-asset]").forEach(img => {
  img.src = assetRoot + encodeURIComponent(img.dataset.asset);
});

const tabs = Array.from(document.querySelectorAll('[role="tab"]'));
function activateTab(tab, focus = false) {
  tabs.forEach(item => {
    const active = item === tab;
    item.setAttribute("aria-selected", String(active));
    item.tabIndex = active ? 0 : -1;
    document.getElementById(item.getAttribute("aria-controls")).hidden = !active;
  });
  if (focus) tab.focus();
}
tabs.forEach((tab, index) => {
  tab.addEventListener("click", () => activateTab(tab));
  tab.addEventListener("keydown", event => {
    let next;
    if (event.key === "ArrowRight") next = (index + 1) % tabs.length;
    if (event.key === "ArrowLeft") next = (index - 1 + tabs.length) % tabs.length;
    if (event.key === "Home") next = 0;
    if (event.key === "End") next = tabs.length - 1;
    if (next !== undefined) {
      event.preventDefault();
      activateTab(tabs[next], true);
    }
  });
});

const dialog = document.getElementById("image-dialog");
const expandedImage = document.getElementById("dialog-image");
const expandedCaption = document.getElementById("dialog-caption");
let imageTrigger;
document.querySelectorAll(".zoom").forEach(button => {
  button.addEventListener("click", () => {
    const original = button.querySelector("img");
    expandedImage.src = original.src;
    expandedImage.alt = original.alt;
    expandedCaption.textContent = button.dataset.caption;
    imageTrigger = button;
    if (typeof dialog.showModal === "function") dialog.showModal();
    else window.open(original.src, "_blank", "noopener");
  });
});
document.querySelector(".dialog-close").addEventListener("click", () => dialog.close());
dialog.addEventListener("click", event => {
  if (event.target !== dialog) return;
  const rect = dialog.getBoundingClientRect();
  if (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom) dialog.close();
});
dialog.addEventListener("close", () => imageTrigger?.focus());
