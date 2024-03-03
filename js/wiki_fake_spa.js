// Insane hack ahead
"use strict";

(function() {
    if (localStorage && localStorage.getItem("wiki_fake_spa_opt_out")) return;

    let contentFrame = document.getElementsByName("bottom")[0];

    let myDir = location.pathname;
    myDir = myDir.substring(0, myDir.lastIndexOf("/")) + "/";

    function changeHash() {
        let bottomLocation = contentFrame.contentWindow.location;
        if (bottomLocation.host != location.host || !bottomLocation.pathname.startsWith(myDir))
            return;
        let relUrl = "#" + bottomLocation.pathname.substring(myDir.length) + bottomLocation.search + bottomLocation.hash;
        if (decodeURI(location.hash) != relUrl) {
            const url = new URL(location);
            url.hash = relUrl;
            history.pushState({}, "", url);
        }
    }
    function changeBottomUrl() {
        let here = location.protocol + "//" + location.host + myDir;
        let bottomLocation = contentFrame.contentWindow.location;
        let dest = location.hash.substring(1);
        let what = new URL(dest, here), whatMutated = new URL(dest, here);
        if (!dest.includes("/") && !whatMutated.pathname.endsWith(".html")) whatMutated.pathname += ".html";
        if (what.href != bottomLocation.href && whatMutated.href != bottomLocation.href)
            bottomLocation.replace(what);
    }

    if (location.hash)
        changeBottomUrl();
    window.addEventListener("hashchange", changeBottomUrl);
    contentFrame.addEventListener("load", () => {
        if (!contentFrame.contentDocument)
            return;
        changeHash();
        contentFrame.contentWindow.addEventListener("popstate", changeHash);
    });
})();