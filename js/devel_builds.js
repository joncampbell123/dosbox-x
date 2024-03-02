/*
  * SPDX-License-Identifier: CC-BY-SA-4.0
  *
  * Copyright (C) 2022-2023 The DOSBox Staging Team
  * Copyright (C) 2023-2024 The DOSBox-X Team
*/

/*
  * Helper functions for populating the contents of the build
  * status table in devel-build.html
*/

function report_error(msg, err, show_alert=false, show_trace=false) {
    let stack = err ? "\n" + (err instanceof Error ? err.stack : err) : "";
    if (err)
        console.error(err);
    console.info(msg);
    if (show_alert)
        alert(msg + (show_trace ? stack : ""));
}

async function add_ci_build_entry(repo, workflow_id, description) {
    let builds = document.querySelector("#builds");
    let build_entry_id = "build-" + workflow_id;
    let build_entry = builds.querySelector("#" + build_entry_id);

    if (!build_entry) {
        build_entry = document.importNode(document.querySelector("#build-template").content.querySelector("tr"), true);
        build_entry.setAttribute("id", build_entry_id);
        builds.appendChild(build_entry);
        let build_name_el = build_entry.querySelector(".build-loading-name");
        build_name_el.innerText = description;
    }

    function report_error_as_build_link(text) {
        let build_status_el = build_entry.querySelector(".build-loading-status");
        build_status_el.style.color = "red";
        build_status_el.innerText = text;
    }

    const query_url = new URL("https://api.github.com/repos/" + repo + "/" + "actions/workflows/" + workflow_id + ".yml" + "/runs");
    query_url.searchParams.set("page", 1);
    query_url.searchParams.set("per_page", 1);
    query_url.searchParams.set("branch", "master");
    query_url.searchParams.set("event", "push");
    query_url.searchParams.set("status", "success");

    let response;
    try {
        response = await fetch(query_url.href);
    } catch (err) {
        report_error(`Error ocurred for "${description}" :-S`, err);
        report_error_as_build_link("Network error");
        return;
    }
    let data;
    async function parse_response(handle_err) {
        try {
            data = await response.json();
        } catch (err) {
            if (!handle_err) throw err;
            report_error(`Response to query of "${description}" can't be parsed as JSON`, err);
            report_error_as_build_link("Response invalid");
            return;
        }
    }

    // Handle HTTP error
    if (!response.ok) {
        function show_generic_error(err) {
            report_error(`Looks like there was a problem while getting the info of "${description}". ` +
            `Status code: ${response.status}`, err);
            report_error_as_build_link(`HTTP error (status code: ${response.status})`);
        }
        if (response.status == 403) {
            try {
                await parse_response(false);
                if (data.message && data.message.includes("API rate limit exceeded")) {
                    report_error(`GitHub API rate limit while getting the info of "${description}".`, data);
                    report_error_as_build_link("API rate limit exceeded");
                } else {
                    show_generic_error(data);
                }
            } catch (err) {
                show_generic_error(err);
            }
        } else {
            show_generic_error();
        }
        return;
    }

    await parse_response();
    const run = data.workflow_runs.length && data.workflow_runs[0];
    if (!run) {
        report_error(`No builds found for "${description}"`, data);
        report_error_as_build_link("No builds found");
        return;
    }

    // Setup row contents
    let build_link = document.createElement("a");
    build_link.setAttribute("href", run.html_url);
    build_link.textContent = description;
    build_entry.querySelector(".build-link")
        .replaceChildren(build_link);

    let build_commit = run.head_commit.id;
    let build_commit_link = document.createElement("a");
    build_commit_link.setAttribute("href", `https://github.com/${repo}/commit/${build_commit}`);
    build_commit_link.textContent = build_commit.substring(0, 7);
    build_entry.querySelector(".build-commit")
        .replaceChildren(build_commit_link);

    build_entry.querySelector(".build-date")
        .textContent = new Date(run.updated_at).toUTCString();
}