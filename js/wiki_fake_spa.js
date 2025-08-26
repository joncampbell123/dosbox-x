// Insane hack ahead
"use strict";

(function() {
    let frame = document.querySelector("main").appendChild(document.querySelector("#empty-iframe").content.querySelector("iframe"));

    if (localStorage && localStorage.getItem("wiki_fake_spa_opt_out")) {
        window.open("./Home", frame.name);
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

        const url = new URL(location);
        url.hash = relUrl;
        if (location.hash != url.hash) {
            // We can't go by the actual page title because they usually just contain the text of the top
            // header
            document.title = decodeURIComponent(strippedPathname).replaceAll("-", " ") + " - DOSBox-X Wiki";
            history.replaceState({}, "", url);
        }

        return false;
    }

    function changeBottomUrl() {
        let bottomLocation = frame.contentWindow.location;
        let newURL = location;
        let dest = contentDir + newURL.hash.substring(1);
        let destUrl = new URL(dest, serverRoot), destUrlWithHtmlExt = destUrl;
        if (destUrl.host != newURL.host || !destUrl.pathname.startsWith(contentDir)) {
            console.error(`Did not navigate to ${dest} as it escapes ${serverRoot}${contentDir}.`);
            return false;
        }
        if (!dest.includes(".") && !destUrlWithHtmlExt.pathname.endsWith(".html"))
            destUrlWithHtmlExt.pathname += ".html";
        try {
            if (destUrl.href != bottomLocation.href && destUrlWithHtmlExt.href != bottomLocation.href) {
                let destUrlNoHash = new URL(destUrl),
                    destUrlWithHtmlExtNoHash = new URL(destUrlWithHtmlExt),
                    bottomLocationNoHash = new URL(bottomLocation.href);
                destUrlNoHash.hash = "";
                destUrlWithHtmlExtNoHash.hash = "";
                bottomLocationNoHash.hash = "";
                if (destUrlNoHash.href == bottomLocationNoHash.href
                    || destUrlWithHtmlExtNoHash.href == bottomLocationNoHash.href)
                    bottomLocation.hash = destUrl.hash;
                else
                    bottomLocation.href = destUrl.href;
            }
        } catch (err) {
            console.error(`Could not navigate to ${dest}.`, err);
            return false;
        }
        return true;
    }

    if (!location.hash || !changeBottomUrl())
        window.open("./Home", frame.name);
    window.addEventListener("hashchange", changeBottomUrl);
    frame.addEventListener("load", () => {
        document.querySelector("#wiki-try-reloading").style.opacity = "0.25";
        if (!frame.contentDocument)
            return;
        changeHash();
        frame.contentWindow.addEventListener("popstate", changeHash);
        if (frame.contentWindow.navigation) {
            frame.contentWindow.navigation.addEventListener("navigate", (e) => {
                const url = new URL(e.destination.url);
                if (url.host != location.host || !url.pathname.startsWith(contentDir)) {
                    e.preventDefault();
                    window.open(url, "_blank");
                }
            });
        }
    });
})();