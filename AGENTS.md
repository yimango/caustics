# Agents

## Pull request evaluation

When a pull request is ready, start a separate evaluation agent. The author does not review their own pull request.

The evaluation agent runs `/code-review` on that pull request. It looks for bugs, behavioral regressions, security issues, and missing tests, then decides whether to merge.

Merge when the review finds nothing that would mislead a user or change behavior incorrectly.

If it does not merge, it sends the findings to the agent that opened the pull request. That agent fixes them and pushes. The evaluation agent reviews the new head. Repeat until it merges, or until a finding needs a decision from the user.
