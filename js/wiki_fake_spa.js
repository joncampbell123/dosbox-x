// Insane hack ahead
"use strict";

(function() {

    if (localStorage.getItem("wiki_fake_spa_opt_out")) return;

    let contentFrame = document.getElementsByName("bottom")[0];

    let myDir = location.pathname;
    myDir = myDir.substring(0, myDir.lastIndexOf("/")) + "/";

    function changeHash(e) {
        let theirWindow = contentFrame.contentWindow;
        if (theirWindow.location.host != location.host) return;
        if (!theirWindow.location.pathname.startsWith(myDir)) return;
        let relUrl = "#" + theirWindow.location.pathname.substring(myDir.length) + theirWindow.location.search + theirWindow.location.hash;
        if (location.hash != relUrl) {
            const url = new URL(location);
            url.hash = relUrl;
            history.pushState({}, "", url);
        }
    }
    function changeBottomUrl(e) {
        if (e && contentFrame.contentDocument.readyState != "complete") return;
        let here = location.protocol + "//" + location.host + myDir;
        let bottomLocation = contentFrame.contentWindow.location;
        let dest = location.hash.substring(1);
        let what = new URL(dest, here), whatMutated = new URL(dest, here);
        if (!dest.includes("/") && !whatMutated.pathname.endsWith(".html")) whatMutated.pathname += ".html";
        if (what.href != bottomLocation.href && whatMutated.href != bottomLocation.href) bottomLocation.replace(what);
    }

    if (location.hash) {
        changeBottomUrl();
    }
    window.addEventListener("hashchange", changeBottomUrl);
    contentFrame.addEventListener("load", () => {
        if (contentFrame.contentDocument) {
            contentFrame.contentWindow.addEventListener("popstate", changeHash);
            contentFrame.contentWindow.addEventListener("hashchange", changeHash);
            changeHash();
        }
    });

})();