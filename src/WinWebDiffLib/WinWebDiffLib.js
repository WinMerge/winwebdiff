(function () {
  if (window.wwd)
    return;
  window.wwd = { "inClick": false };
  function getNearestVisibleElement(el) {
    while (el) {
      const rc = el.getBoundingClientRect();
      if (rc.left == 0 && rc.top == 0 && rc.width == 0 && rc.height == 0) {
        el = el.nextElementSibling ? el.nextElementSibling : el.parentElement;
        continue;
      }
      break;
    }
    return el;
  }
  function getScrollPositionFromHint(el, msg, hint) {
    const rleft = Math.round((el.scrollWidth - el.clientWidth) * msg.rleft);
    let rtop;
    let ydistance;
    let el1 = hint.selector[0] != "" ? el.querySelector(hint.selector[0]) : null;
    if (el1) {
      el1 = getNearestVisibleElement(el1);
    }
    let el2 = hint.selector[1] != "" ? el.querySelector(hint.selector[1]) : null;
    if (el2) {
      el2 = getNearestVisibleElement(el2);
    }
    const rectParent = el.getBoundingClientRect();
    let rectParentTop = rectParent.top;
    if (el.localName !== "html" && el.localName !== "body")
      rectParentTop -= el.scrollTop;
    if (!el1 && !el2) {
      rtop = Math.round((el.scrollHeight - el.clientHeight) * msg.rtop);
      ydistance = el.scrollHeight;
    } else if (!el1 && el2) {
      const rect2 = el2.getBoundingClientRect();
      const ry = (hint.yc - 0) / (hint.y2 - 0);
      rtop = Math.round(0 + (rect2.top - rectParentTop) * ry - hint.yo);
      ydistance = rect2.top - rectParentTop;
    } else if (el1 && !el2) {
      const rect1 = el1.getBoundingClientRect();
      const ry = (hint.yc - hint.y1) / (hint.scrollHeight - hint.y1);
      rtop = Math.round((rect1.top + rect1.height - rectParentTop) +
            (el.scrollHeight - (rect1.top + rect1.height - rectParentTop)) * ry - hint.yo);
      ydistance = (el.scrollHeight - (rect1.top + rect1.height - rectParentTop));
    } else if (el1 == el2) {
      const rect1 = el1.getBoundingClientRect();
      const ry = (hint.yc - hint.y1) / (hint.y2 - hint.y1);
      rtop = Math.round(rect1.top - rectParentTop + rect1.height * ry - hint.yo);
      ydistance = rect1.height;
    } else {
      const rect1 = el1.getBoundingClientRect();
      const rect2 = el2.getBoundingClientRect();
      const ry = (hint.yc - hint.y1) / (hint.y2 - hint.y1);
      rtop = Math.round(rect1.top + rect1.height - rectParentTop + (rect2.top - (rect1.top + rect1.height)) * ry - hint.yo);
      ydistance = rect2.top - (rect1.top + rect1.height);
    }
    return [ rleft, rtop, 0, ydistance ]
  }
  function getScrollPosition(el, msg) {
    const [ rleft1, rtop1,, ydistance1 ] = getScrollPositionFromHint(el, msg, msg.nearestDiffs);
    const [ rleft2, rtop2,, ydistance2 ] = getScrollPositionFromHint(el, msg, msg.nearestElementsHavingId);
    if (ydistance1 < ydistance2)
      return [rleft1, rtop1];
    return [rleft2, rtop2];
  }
  function getWindowLocation() {
    let locationString = "";
    let currentWindow = window;

    while (currentWindow !== window.top) {
      const frames = currentWindow.parent.frames;
      let index = -1;
      for (let i = 0; i < frames.length; i++) {
        if (frames[i] === currentWindow) {
          index = i;
          break;
        }
      }
      if (index !== -1) {
        locationString = `[${index}]` + locationString;
      } else {
        locationString = "top" + locationString;
      }
      currentWindow = currentWindow.parent;
    }

    return locationString;
  }
  function getElementSelector(element) {
    if (!(element instanceof Element)) {
      return null;
    }

    const selectorList = [];
    while (element.parentNode) {
      let nodeName = element.nodeName.toLowerCase();
      if (element.id) {
        const selector = element.id.replaceAll(/([\.#,\[\]\:>\+\-\*])/g, "\\$1");
        selectorList.unshift(`#${selector}`);
        break;
      } else {
        let sibCount = 0;
        let sibIndex = 0;
        const siblings = element.parentNode.childNodes;
        for (let i = 0; i < siblings.length; i++) {
          const sibling = siblings[i];
          if (sibling.nodeType === 1) {
            if (sibling === element) {
              sibIndex = sibCount;
            }
            if (sibling.nodeName.toLowerCase() === nodeName) {
              sibCount++;
            }
          }
        }
        if (sibIndex > 0) {
          nodeName += `:nth-of-type(${sibIndex + 1})`;
        }
        selectorList.unshift(nodeName);
        element = element.parentNode;
      }
    }
    return selectorList.join(" > ");
  }
  function isElementFixed(el, elRoot) {
    while (el && elRoot != el) {
      const sty = window.getComputedStyle(el);
      if (sty.position === 'fixed' || sty.position === 'sticky')
        return true;
      el = el.parentElement;
    }
    return false;
  }
  function hasScrollableAncestor(el, elRoot) {
    let ancestor = el.parentElement;
    while (ancestor && elRoot != ancestor) {
      if (ancestor.scrollHeight - ancestor.clientHeight > 4) {
        const sty = window.getComputedStyle(ancestor);
        if (sty.overflow === 'auto' || sty.overflow === 'scroll')
          return true;
      }
      ancestor = ancestor.parentElement;
    }
    return false;
  }
  function getNearestDiffs(elScroll) {
    const scrTop = elScroll.scrollTop;
    let id1 = -1, id2 = -1;
    let y1 = 0, y2 = elScroll.scrollHeight;
    let rectParent = elScroll.getBoundingClientRect();
    let rectParentTop = rectParent.top;
    const yc = scrTop + elScroll.clientHeight / 2;
    if (elScroll.localName !== "html" && elScroll.localName !== "body")
      rectParentTop -= scrTop;
    for (let el of elScroll.getElementsByClassName("wwd-diff")) {
      const rect = el.getBoundingClientRect();
      if (rect.left == 0 && rect.top == 0 && rect.width == 0 && rect.height == 0)
        continue;
      if (hasScrollableAncestor(el, elScroll))
        continue;
      if (isElementFixed(el, elScroll))
        continue;
      const id = el.dataset["wwdid"];
      const t = rect.top - rectParentTop;
      const b = t + rect.height;
      if (t <= yc && yc <= b) {
        id1 = id;
        id2 = id;
        y1 = t;
        y2 = b;
        break;
      }
      if (b < yc && b >= y1) {
        id1 = id;
        y1 = b;
      }
      if (t > yc && t < y2) {
        id2 = id;
        y2 = t;
      }
    }
    return {
      "selector":
         [id1 == -1 ? "" : "[data-wwdid='" + id1 + "']",
          id2 == -1 ? "" : "[data-wwdid='" + id2 + "']"],
      "y1": y1,
      "y2": y2,
      "yc": yc,
      "yo": yc - scrTop,
      "scrollHeight": elScroll.scrollHeight
    };
  }
  function getNearestElementsHavingId(elScroll) {
    const scrTop = elScroll.scrollTop;
    let id1 = "", id2 = "";
    let y1 = 0, y2 = elScroll.scrollHeight;
    let rectParent = elScroll.getBoundingClientRect();
    let rectParentTop = rectParent.top;
    const yc = scrTop + elScroll.clientHeight / 2;
    if (elScroll.localName !== "html" && elScroll.localName !== "body")
      rectParentTop -= scrTop;
    for (let el of elScroll.querySelectorAll("[id]")) {
      const rect = el.getBoundingClientRect();
      if (rect.left == 0 && rect.top == 0 && rect.width == 0 && rect.height == 0)
        continue;
      if (hasScrollableAncestor(el, elScroll))
        continue;
      if (isElementFixed(el))
        continue;
      const id = el.id;
      const t = rect.top - rectParentTop;
      const b = t + rect.height;
      if (t <= yc && yc <= b) {
        id1 = id;
        id2 = id;
        y1 = t;
        y2 = b;
        break;
      }
      if (b < yc && b >= y1) {
        id1 = id;
        y1 = b;
      }
      if (t > yc && t < y2) {
        id2 = id;
        y2 = t;
      }
    }
    return {
      "selector":
         [id1 == "" ? "" : "[id='" + id1 + "']",
          id2 == "" ? "" : "[id='" + id2 + "']"],
      "y1": y1,
      "y2": y2,
      "yc": yc,
      "yo": yc - scrTop,
      "scrollHeight": elScroll.scrollHeight
    };
  }
  function getScrollableAncestor(el, elRoot) {
    let ancestor = el.parentElement;
    while (ancestor && elRoot != ancestor) {
      if (ancestor.scrollHeight - ancestor.clientHeight > 4) {
        const sty = window.getComputedStyle(ancestor);
        if (sty.overflow === 'auto' || sty.overflow === 'scroll')
          return ancestor;
      }
      ancestor = ancestor.parentElement;
    }
    return elRoot;
  }
  function makeContainerInfo(mapContainer, container) {
    let containerId = -1;
    if (container !== document.documentElement) {
      const containerContainer = getScrollableAncestor(container, document.documentElement);
      if (!mapContainer.has(containerContainer)) {
        makeContainerInfo(mapContainer, containerContainer);
      }
      containerId = parseInt(mapContainer.get(containerContainer).id);
    }
    const containerRect = container.getBoundingClientRect();
    const containerInfo = {
      "id": mapContainer.size,
      "containerId": containerId,
      "left": containerRect.left,
      "top": containerRect.top,
      "width": containerRect.width,
      "height": containerRect.height,
      "scrollWidth": container.scrollWidth,
      "scrollHeight": container.scrollHeight,
      "scrollLeft": container.scrollLeft,
      "scrollTop": container.scrollTop,
      "clientWidth": container.clientWidth,
      "clientHeight": container.clientHeight
    };
    if (!mapContainer.has(container)) {
      mapContainer.set(container, containerInfo);
    }
    return containerInfo;
  }
  function getFrameRects() {
    let frames = {};
    const iframes = document.getElementsByTagName("iframe");
    for (let i = 0; i < window.frames.length; i++) {
      for (let iframe of iframes) {
        if (iframe.contentWindow == window.frames[i]) {
          const rect = iframe.getBoundingClientRect();
          frames[getWindowLocation() + "[" + i + "]"] = { "left": rect.left, "top": rect.top, "width": rect.width, "height": rect.height };
        }
      }
    }
    return frames;
  }
  function getDiffRects() {
    let mapContainer = new Map();
    let diffRects = new Array();
    let containerRects = new Array();
    const containerRoot = document.documentElement;
    const containerInfoRoot = makeContainerInfo(mapContainer, containerRoot);
    mapContainer.set(containerRoot, containerInfoRoot);
    containerRects.push(containerInfoRoot);
    for (let el of document.getElementsByClassName("wwd-diff")) {
      const rect = el.getBoundingClientRect();
      if (rect.left == 0 && rect.top == 0 && rect.width == 0 && rect.height == 0)
        continue;
      const container = getScrollableAncestor(el, document.documentElement);
      if (!mapContainer.has(container)) {
        const containerInfo = makeContainerInfo(mapContainer, container);
        containerRects.push(containerInfo);
      }
      const id = parseInt(el.dataset["wwdid"]);
      const info = {
        "id": id,
        "containerId": parseInt(mapContainer.get(container).id),
        "left": rect.left,
        "top": rect.top,
        "width": rect.width,
        "height": rect.height
      };
      diffRects.push(info);
    }
    return {
      "event": "diffRects",
      "window": getWindowLocation(),
      "diffRects": diffRects,
      "containerRects": containerRects,
      "frameRects": getFrameRects(),
      "scrollX": window.scrollX,
      "scrollY": window.scrollY,
      "clientWidth": document.scrollingElement.clientWidth,
      "clientHeight": document.scrollingElement.clientHeight
    };
  }
  function syncScroll(msg) {
    const el = document.querySelector(msg.selector);
    if (el && getWindowLocation() === msg.window) {
      clearTimeout(wwd.timeout);
      wwd.timeout = setTimeout(function () {
        const [rleft, rtop] = getScrollPosition(el, msg);
        if (el.scroll) {
          window.removeEventListener("scroll", onScroll, true);
          el.scroll(rleft, rtop);
          setTimeout(function () { window.addEventListener("scroll", onScroll, true); }, 10);
        }
      }, 100);
    }
  }
  function syncClick(msg) {
    var el;
    if (msg.href) {
      const ary = msg.href.split('/');
      for (let i = 0; i < ary.length; i++) {
        let sel;
        for (let removeURLQueryParams of [false, true]) {
          if (removeURLQueryParams)
            sel = "[href*='" + ary.slice(i).join('/').replace(/\?.*$/, '') + "']";
          else
            sel = "[href$='" + ary.slice(i).join('/') + "']";
          const els = document.querySelectorAll(sel);
          if (els.length >= 1) {
            el = els[0];
            break;
          }
        }
        if (el)
          break;
      }
    }
    if (!el)
      el = document.querySelector(msg.selector);
    if (el && getWindowLocation() === msg.window) {
      wwd.inClick = true;
      if (el.click)
        el.click();
      wwd.inClick = false;
    }
  }
  function syncInput(msg) {
    var el = document.querySelector(msg.selector);
    if (el && getWindowLocation() === msg.window) {
      el.value = msg.value;
    }
  }
  function onScroll(e) {
    const elScroll = ("scrollingElement" in e.target) ? e.target.scrollingElement : e.target;
    clearTimeout(wwd.timeout2);
    wwd.timeout2 = setTimeout(function () {
      const sel = getElementSelector(elScroll);
      const nearestDiffs = getNearestDiffs(elScroll);
      const nearestElementsHavingId = getNearestElementsHavingId(elScroll);
      const msg = {
        "event": "scroll",
        "window": getWindowLocation(),
        "selector": sel,
        "rleft": ((elScroll.scrollWidth == elScroll.clientWidth) ? 0 : (elScroll.scrollLeft / (elScroll.scrollWidth - elScroll.clientWidth))),
        "rtop": ((elScroll.scrollHeight == elScroll.clientHeight) ? 0 : (elScroll.scrollTop / (elScroll.scrollHeight - elScroll.clientHeight))),
        "nearestDiffs": nearestDiffs,
        "nearestElementsHavingId": nearestElementsHavingId
      };
      window.chrome.webview.postMessage(JSON.stringify(msg));
    }, 100);
  }
  function onClick(e) {
    if (wwd.inClick)
      return;
    let te = e.target;
    while (te.classList.contains("wwd-wdiff") || te.classList.contains("wwd-diff"))
      te = te.parentElement;
    const sel = getElementSelector(te);
    const msg = { "event": "click", "window": getWindowLocation(), "selector": sel, "href": te.href };
    window.chrome.webview.postMessage(JSON.stringify(msg));
  }
  function onInput(e) {
    const sel = getElementSelector(e.target);
    const msg = { "event": "input", "window": getWindowLocation(), "selector": sel, "value": e.target.value };
    window.chrome.webview.postMessage(JSON.stringify(msg));
  }
  function onDblClick(e) {
    const el = e.target;
    const sel = getElementSelector(el);
    const wwdid = ("wwdid" in el.dataset) ? el.dataset["wwdid"] : (("wwdid" in el.parentElement.dataset) ? el.parentElement.dataset["wwdid"] : -1);
    const msg = { "event": "dblclick", "window": getWindowLocation(), "selector": sel, "wwdid": parseInt(wwdid) };
    window.chrome.webview.postMessage(JSON.stringify(msg));
  }
  function onMessage(arg) {
    const data = arg.data;
    switch (data.event) {
      case "scroll":
        syncScroll(data);
        break;
      case "click":
        syncClick(data);
        break;
      case "input":
        syncInput(data);
        break;
      case "diffRects":
        clearTimeout(wwd.timeout3);
        wwd.timeout3 = setTimeout(function () {
          window.chrome.webview.postMessage(JSON.stringify(getDiffRects()));
        }, 300);
        break;
      case "scrollTo":
        if (getWindowLocation() === data.window) {
          window.scrollTo(data.scrollX, data.scrollY);
        }
        break;
    }
  }
  window.addEventListener("click", onClick, true);
  window.addEventListener("input", onInput, true);
  window.addEventListener("dblclick", onDblClick, true);
  window.addEventListener("scroll", onScroll, true);
  window.chrome.webview.addEventListener("message", onMessage);
})();
