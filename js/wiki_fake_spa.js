// Insane hack ahead
"use strict";

(function() {
    let frame = document.querySelector("main").appendChild(document.querySelector("#empty-iframe").content.querySelector("iframe"));

    if (localStorage && localStorage.getItem("wiki_fake_spa_opt_out")) {
        frame.contentWindow.location.href = "./Home";
        return;
    }

    const serverRoot = location.protocol + "//" + location.host;
    const contentDir = location.pathname.substring(0, location.pathname.lastIndexOf("/") + 1);

    function changeHash() {
        let bottomLocation = frame.contentWindow.location;
        if (bottomLocation.host != location.host || !bottomLocation.pathname.startsWith(contentDir))
            return;
        let strippedPathname = bottomLocation.pathname.substring(contentDir.length)
            .replace(/(?:.html)?(?:\/)?$/, "");
        // FIXME: Is this even correct?
        let relUrl = "#" + encodeURI(decodeURIComponent(
            strippedPathname + bottomLocation.search + bottomLocation.hash
        ));
        // We can't go by the actual page title because they usually just contain the text of the top
        // header
        document.title = decodeURIComponent(strippedPathname).replaceAll("-", " ") + " - DOSBox-X Wiki";
        if (location.hash != relUrl) {
            const url = new URL(location);
            url.hash = relUrl;
            history.pushState({}, "", url);
        }
    }
    function changeBottomUrl() {
        let bottomLocation = frame.contentWindow.location;
        let dest = contentDir + location.hash.substring(1);
        let destUrl = new URL(dest, serverRoot), destUrlWithHtmlExt = destUrl;
        if (destUrl.host != location.host || !destUrl.pathname.startsWith(contentDir)) {
            console.error(`Did not navigate to ${dest} as it escapes ${serverRoot}${contentDir}.`);
            return false;
        }
        if (!dest.includes(".") && !destUrlWithHtmlExt.pathname.endsWith(".html"))
            destUrlWithHtmlExt.pathname += ".html";
        try {
            if (destUrl.href != bottomLocation.href && destUrlWithHtmlExt.href != bottomLocation.href)
                bottomLocation.replace(destUrl);
        } catch (err) {
            console.error(`Could not navigate to ${dest}.`, err);
            return false;
        }
        return true;
    }

    if (!location.hash || !changeBottomUrl())
        frame.contentWindow.location.href = "./Home";
    window.addEventListener("hashchange", changeBottomUrl);
    frame.addEventListener("load", () => {
        if (!frame.contentDocument)
            return;
        document.querySelector("#wiki-try-reloading").style.opacity = "0.25";
        changeHash();
        frame.contentWindow.addEventListener("popstate", changeHash);
    });
})();