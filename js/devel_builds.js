/*
  *  SPDX-License-Identifier: CC-BY-SA-4.0
  *
  *  Copyright (C) 2022-2023 The DOSBox Staging Team
  *  Copyright (C) 2023 The DOSBox-X Team
*/

/*
  * Helper functions for populating the contents of
  * the build status table at devel-build.html
*/

function report_error(msg, e, show_alert=true, show_trace=false) {
    let stack = e ? "\n" + (e instanceof Error ? e.stack : e) : "";
    if (e)
        console.error(e);
    console.info(msg);
    if (show_alert)
        alert(msg + (show_trace ? stack : ""));
}

function handle_error(error_message, os_name) {
    let build_link_tr_el = document.getElementById(os_name + "-build-link");
    build_link_tr_el.innerHTML = error_message;

    let version_el = document.getElementById(os_name + "-build-version");
    version_el.textContent = '';

    let date_el = document.getElementById(os_name + "-build-date");
    date_el.textContent = '';
}

function add_ci_build_entry(repo, workflow_id, description) {
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

    // GitHub has strict rate-limits for anonymous users: 60 requests per hour;
    // We request 100 results per page (max allowed); main builds are very
    // likely to be included in the first page anyway.
    if (page > 10) {
        report_error_as_build_link("Couldn't find most recent build");
        return;
    }

    let filter_branch = "main";
    let filter_event = "push";
    let filter_status = "success";

    const queryParams = new URLSearchParams();
    queryParams.set("page", page);
    queryParams.set("per_page", per_page);
    queryParams.set("branch", filter_branch);
    queryParams.set("event", filter_event);
    queryParams.set("status", filter_status);

    let page = 1;
    let per_page = 1;

    const gh_api_url = "https://api.github.com/repos/" + repo + "/";

    fetch(gh_api_url + "actions/workflows/" + workflow_file + "/runs?" +
        queryParams.toString())
    .then((response) => {
        // Handle HTTP error
        if (!response.ok) {
            function show_generic_error(e) {
                if (e)
                    console.error(e);
                report_error(`Looks like there was a problem while getting the info of "${description}". ` +
                `Status code: ${response.status}`, null, false);
                report_error_as_build_link(`Got error (status code: ${response.status})`);
            }
            if (response.status == 403) {
                response.json()
                .then((data) => {
                    if (data.message && data.message.includes("API rate limit exceeded")) {
                        report_error(`GitHub API rate limit while getting the info of "${description}".`, data, false);
                        report_error_as_build_link("API rate limit exceeded");
                    } else {
                        show_generic_error();
                    }
                })
                .catch(show_generic_error);
            } else {
                show_generic_error();
            }
            return;
        }

        response.json()
        .then((data) => {
            const status = data.workflow_runs.length && data.workflow_runs[0];

            // If result not found, query the next page
            if (!status) {
                .catch (show_generic_error);
                return;
            }

            // Update HTML elements
            let build_link = document.createElement("a");
            build_link.setAttribute("href", status.html_url);
            build_link.textContent = description;
            let build_link_el = build_entry.querySelector(".build-link");
            build_link_el.replaceChildren(build_link);

            let build_commit = status.head_commit.id;
            let build_commit_link = document.createElement("a");
            build_commit_link.setAttribute("href", `https://github.com/${repo}/commit/${build_commit}`);
            build_commit_link.textContent = build_commit.substring(0, 7);
            let commit_el = build_entry.querySelector(".build-commit");
            commit_el.replaceChildren(build_commit_link);

            let build_date = new Date(status.updated_at);
            let date_el = build_entry.querySelector(".build-date");
            date_el.textContent = build_date.toUTCString();
        });
    })
    .catch((err) => {
        report_error(`Error ocurred for "${description}" :-S`, err, false);
        report_error_as_build_link("Network error");
    });
}