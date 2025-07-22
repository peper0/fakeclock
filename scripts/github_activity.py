#!/usr/bin/env python3
"""Fetch GitHub issues and pull request comments for the FakeClock repo."""

import json
import urllib.request
from typing import Any, List

REPO = "peper0/fakeclock"
GITHUB_API = "https://api.github.com/repos/{}".format(REPO)


def fetch_json(url: str) -> Any:
    """Return parsed JSON data from the given URL."""
    with urllib.request.urlopen(url) as resp:
        data = resp.read()
    return json.loads(data.decode())


def list_issues() -> None:
    """Print open issue numbers and titles."""
    issues = fetch_json(f"{GITHUB_API}/issues")
    print("Open issues:")
    for issue in issues:
        # Skip pull requests (they also appear in /issues)
        if "pull_request" in issue:
            continue
        print(f"- #{issue['number']}: {issue['title']}")


def list_pr_comments(pr_number: int) -> None:
    """Print comments for a given pull request."""
    issue_comments = fetch_json(f"{GITHUB_API}/issues/{pr_number}/comments")
    review_comments = fetch_json(f"{GITHUB_API}/pulls/{pr_number}/comments")
    for c in issue_comments:
        user = c.get("user", {}).get("login", "")
        body = c.get("body", "").splitlines()[0]
        print(f"  Issue comment by {user}: {body}")
    for c in review_comments:
        user = c.get("user", {}).get("login", "")
        body = c.get("body", "").splitlines()[0]
        print(f"  Review comment by {user}: {body}")


def list_prs() -> List[int]:
    """Return a list of open pull request numbers with titles."""
    prs = fetch_json(f"{GITHUB_API}/pulls")
    print("\nOpen pull requests:")
    numbers = []
    for pr in prs:
        num = pr["number"]
        numbers.append(num)
        print(f"- #{num}: {pr['title']}")
    return numbers


def main() -> None:
    list_issues()
    pr_numbers = list_prs()
    for num in pr_numbers:
        print(f"\nComments for PR #{num}:")
        list_pr_comments(num)


if __name__ == "__main__":
    main()
