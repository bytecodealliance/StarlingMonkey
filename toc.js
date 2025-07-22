// Populate the sidebar
//
// This is a script, and not included directly in the page, to control the total size of the book.
// The TOC contains an entry for each page, so if each page includes a copy of the TOC,
// the total size of the page becomes O(n**2).
class MDBookSidebarScrollbox extends HTMLElement {
    constructor() {
        super();
    }
    connectedCallback() {
        this.innerHTML = '<ol class="chapter"><li class="chapter-item expanded affix "><a href="index.html">Introduction</a></li><li class="chapter-item expanded affix "><li class="part-title">Getting Started</li><li class="chapter-item expanded "><a href="getting-started/building.html"><strong aria-hidden="true">1.</strong> Building and Running</a></li><li class="chapter-item expanded "><a href="getting-started/testing.html"><strong aria-hidden="true">2.</strong> Testing</a></li><li class="chapter-item expanded affix "><li class="part-title">Developer Guide</li><li class="chapter-item expanded "><a href="developer/just.html"><strong aria-hidden="true">3.</strong> Project workflow using just</a></li><li class="chapter-item expanded "><a href="developer/builtins-cpp.html"><strong aria-hidden="true">4.</strong> Devoloping Builtins in C++</a></li><li class="chapter-item expanded "><a href="developer/builtins-rust.html"><strong aria-hidden="true">5.</strong> 🚧 Devoloping Builtins in Rust</a></li><li class="chapter-item expanded "><a href="developer/spidermonkey.html"><strong aria-hidden="true">6.</strong>  Devoloping Changes to SpiderMonkey</a></li><li class="chapter-item expanded "><a href="developer/debugging.html"><strong aria-hidden="true">7.</strong> 🚧 Debugging StarlingMonkey application</a></li><li class="chapter-item expanded affix "><li class="part-title">Community</li><li class="chapter-item expanded "><a href="resources/community.html"><strong aria-hidden="true">8.</strong> Community</a></li><li class="chapter-item expanded "><a href="ADOPTERS.html"><strong aria-hidden="true">9.</strong> Adopters</a></li><li class="chapter-item expanded "><a href="CODE_OF_CONDUCT.html"><strong aria-hidden="true">10.</strong> Code of Conduct</a></li></ol>';
        // Set the current, active page, and reveal it if it's hidden
        let current_page = document.location.href.toString().split("#")[0];
        if (current_page.endsWith("/")) {
            current_page += "index.html";
        }
        var links = Array.prototype.slice.call(this.querySelectorAll("a"));
        var l = links.length;
        for (var i = 0; i < l; ++i) {
            var link = links[i];
            var href = link.getAttribute("href");
            if (href && !href.startsWith("#") && !/^(?:[a-z+]+:)?\/\//.test(href)) {
                link.href = path_to_root + href;
            }
            // The "index" page is supposed to alias the first chapter in the book.
            if (link.href === current_page || (i === 0 && path_to_root === "" && current_page.endsWith("/index.html"))) {
                link.classList.add("active");
                var parent = link.parentElement;
                if (parent && parent.classList.contains("chapter-item")) {
                    parent.classList.add("expanded");
                }
                while (parent) {
                    if (parent.tagName === "LI" && parent.previousElementSibling) {
                        if (parent.previousElementSibling.classList.contains("chapter-item")) {
                            parent.previousElementSibling.classList.add("expanded");
                        }
                    }
                    parent = parent.parentElement;
                }
            }
        }
        // Track and set sidebar scroll position
        this.addEventListener('click', function(e) {
            if (e.target.tagName === 'A') {
                sessionStorage.setItem('sidebar-scroll', this.scrollTop);
            }
        }, { passive: true });
        var sidebarScrollTop = sessionStorage.getItem('sidebar-scroll');
        sessionStorage.removeItem('sidebar-scroll');
        if (sidebarScrollTop) {
            // preserve sidebar scroll position when navigating via links within sidebar
            this.scrollTop = sidebarScrollTop;
        } else {
            // scroll sidebar to current active section when navigating via "next/previous chapter" buttons
            var activeSection = document.querySelector('#sidebar .active');
            if (activeSection) {
                activeSection.scrollIntoView({ block: 'center' });
            }
        }
        // Toggle buttons
        var sidebarAnchorToggles = document.querySelectorAll('#sidebar a.toggle');
        function toggleSection(ev) {
            ev.currentTarget.parentElement.classList.toggle('expanded');
        }
        Array.from(sidebarAnchorToggles).forEach(function (el) {
            el.addEventListener('click', toggleSection);
        });
    }
}
window.customElements.define("mdbook-sidebar-scrollbox", MDBookSidebarScrollbox);
