// Insane hack ahead
"use strict";

(function() {
    if (localStorage && localStorage.getItem("wiki_fake_spa_opt_out")) return;

    let frame = document.querySelector("main").appendChild(document.querySelector("#empty-iframe").content.querySelector("iframe"));

    let contentDir = location.pathname;
    contentDir = contentDir.substring(0, contentDir.lastIndexOf("/") + 1);

    function changeHash() {
        let bottomLocation = frame.contentWindow.location;
        if (bottomLocation.host != location.host || !bottomLocation.pathname.startsWith(contentDir))
            return;
        // FIXME: Is this even correct?
        let relUrl = "#" + encodeURI(decodeURIComponent(
            bottomLocation.pathname.substring(contentDir.length) + bottomLocation.search + bottomLocation.hash
        ));
        if (location.hash != relUrl) {
            const url = new URL(location);
            url.hash = relUrl;
            history.pushState({}, "", url);
        }
        document.title = decodeURIComponent(
            bottomLocation.pathname.substring(contentDir.length)
            .replace(/(?:.html)?(?:\/)?$/, "")
        ).replaceAll("-", " ") + " - DOSBox-X Wiki";
    }
    function changeBottomUrl() {
        let bottomLocation = frame.contentWindow.location;
        let dest = contentDir + location.hash.substring(1);
        let serverRoot = location.protocol + "//" + location.host;
        let destUrl = new URL(dest, serverRoot), destUrlWithHtmlExt = destUrl;
        if (!dest.includes("/") && !destUrlWithHtmlExt.pathname.endsWith(".html"))
            destUrlWithHtmlExt.pathname += ".html";
        if (destUrl.href != bottomLocation.href && destUrlWithHtmlExt.href != bottomLocation.href)
            bottomLocation.replace(destUrl);
    }

    if (location.hash)
        changeBottomUrl();
    else
        frame.contentWindow.location.href = "./Home";
    window.addEventListener("hashchange", changeBottomUrl);
    frame.addEventListener("load", () => {
        if (!frame.contentDocument)
            return;
        changeHash();
        frame.contentWindow.addEventListener("popstate", changeHash);
    });
})();