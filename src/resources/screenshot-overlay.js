/* Element picker for the screenshot overlay. Runs in an isolated script world,
 * so the page can neither see nor alter it.
 */
(function () {
  "use strict";

  const strings = window.EphyScreenshotStrings || {};

  const selector = {
    _overlay: null,
    _highlight: null,
    _pointer: null,
    _target: null,
    _frame: 0,

    init() {
      if (!document.body)
        return false;

      this.destroy();

      this._overlay = document.createElement("div");
      this._overlay.id = "ephy-screenshot-overlay";
      this._overlay.tabIndex = -1;
      this._overlay.setAttribute("role", "dialog");
      this._overlay.setAttribute("aria-modal", "true");
      this._overlay.setAttribute("aria-label", strings.title || "");

      this._highlight = document.createElement("div");
      this._highlight.id = "ephy-screenshot-highlight";
      this._overlay.appendChild(this._highlight);

      this._overlay.addEventListener("pointermove", this._onPointerMove);
      this._overlay.addEventListener("click", this._onClick);
      document.addEventListener("scroll", this._onScroll, true);
      document.addEventListener("keydown", this._onKeyDown, true);

      document.body.appendChild(this._overlay);
      this._overlay.focus({ preventScroll: true });

      return true;
    },

    destroy() {
      if (this._frame) {
        cancelAnimationFrame(this._frame);
        this._frame = 0;
      }

      if (this._overlay) {
        this._overlay.removeEventListener("pointermove", this._onPointerMove);
        this._overlay.removeEventListener("click", this._onClick);
        this._overlay.remove();
      }

      document.removeEventListener("scroll", this._onScroll, true);
      document.removeEventListener("keydown", this._onKeyDown, true);

      this._overlay = null;
      this._highlight = null;
      this._pointer = null;
      this._target = null;
    },

    _onPointerMove: (event) => {
      selector._pointer = { x: event.clientX, y: event.clientY };
      if (selector._frame)
        return;

      selector._frame = requestAnimationFrame(() => {
        selector._frame = 0;
        selector._updateTarget();
      });
    },

    _onScroll: () => {
      selector._updateTarget();
    },

    _onKeyDown: (event) => {
      if (event.key !== "Escape")
        return;

      event.preventDefault();
      event.stopPropagation();
      selector._send({ action: "cancel" });
    },

    _onClick: (event) => {
      const target = selector._elementAt(event.clientX, event.clientY);

      /* Clicking past any element, on the dimmed backdrop, cancels. */
      if (!target) {
        selector._send({ action: "cancel" });
        return;
      }

      const rect = target.getBoundingClientRect();
      const root = document.documentElement;

      /* The snapshot covers the viewport, so the element is clipped to it: what
       * the highlight shows is what gets captured. Coordinates are CSS pixels,
       * and the C side derives the device scale from viewWidth and the snapshot
       * width, which beats devicePixelRatio under page zoom.
       */
      const left = Math.max(rect.left, 0);
      const top = Math.max(rect.top, 0);

      selector._send({
        action: "element",
        x: left,
        y: top,
        width: Math.min(rect.right, root.clientWidth) - left,
        height: Math.min(rect.bottom, root.clientHeight) - top,
        viewWidth: root.clientWidth,
      });
    },

    _elementAt(x, y) {
      return document.elementsFromPoint(x, y).find(
        (element) => !this._overlay.contains(element) &&
                     element !== document.body &&
                     element !== document.documentElement);
    },

    _updateTarget() {
      if (!this._pointer || !this._highlight)
        return;

      this._target = this._elementAt(this._pointer.x, this._pointer.y);

      if (!this._target) {
        this._highlight.classList.remove("visible");
        return;
      }

      const rect = this._target.getBoundingClientRect();

      this._highlight.style.left = `${rect.x}px`;
      this._highlight.style.top = `${rect.y}px`;
      this._highlight.style.width = `${rect.width}px`;
      this._highlight.style.height = `${rect.height}px`;
      this._highlight.classList.add("visible");
    },

    /* The overlay is torn down before the message is sent so that it is gone
     * from the document by the time the snapshot is taken.
     */
    _send(payload) {
      this.destroy();

      const handler = window.webkit && window.webkit.messageHandlers &&
                      window.webkit.messageHandlers.screenshot;
      if (handler)
        handler.postMessage(payload);
    },
  };

  if (window.EphyScreenshotSelector)
    window.EphyScreenshotSelector.destroy();

  window.EphyScreenshotSelector = selector;

  return selector.init();
})();
