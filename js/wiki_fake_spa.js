// Insane hack ahead
"use strict";

(function() {
    if (localStorage && localStorage.getItem("wiki_fake_spa_opt_out")) return;

    let frame = document.querySelector("main").appendChild(document.querySelector("#empty-iframe").content.querySelector("iframe"));

    let contentDir = location.pathname;
    contentDir = contentDir.substring(0, contentDir.lastIndexOf("/") + 1);
    let contentDirUrl = location.protocol + "//" + location.host + contentDir;

    function changeHash() {
        let bottomLocation = frame.contentWindow.location;
        if (bottomLocation.host != location.host || !bottomLocation.pathname.startsWith(contentDir))
            return;
        let relUrl = "#" + bottomLocation.pathname.substring(contentDir.length) + bottomLocation.search + bottomLocation.hash;
        if (decodeURI(location.hash) != relUrl) {
            const url = new URL(location);
            url.hash = relUrl;
            history.pushState({}, "", url);
        }
    }
    function changeBottomUrl() {
        let bottomLocation = frame.contentWindow.location;
        let dest = location.hash.substring(1);
        let destUrl = new URL(dest, contentDirUrl), destUrlWithHtmlExt = new URL(dest, contentDirUrl);
        if (!dest.includes("/") && !destUrlWithHtmlExt.pathname.endsWith(".html"))
            destUrlWithHtmlExt.pathname += ".html";
        if (destUrl.href != bottomLocation.href && destUrlWithHtmlExt.href != bottomLocation.href)
            bottomLocation.replace(destUrl);
    }

    if (location.hash)
        changeBottomUrl();
    window.addEventListener("hashchange", changeBottomUrl);
    frame.addEventListener("load", () => {
        if (!frame.contentDocument)
            return;
        changeHash();
        frame.contentWindow.addEventListener("popstate", changeHash);
    });
})();