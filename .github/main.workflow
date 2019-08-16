workflow "New workflow" {
  on = "push"
  resolves = ["GitHub Action for Slack", "GitHub Action for Docker"]
}

action "GitHub Action for Slack" {
  uses = "Ilshidur/action-slack@d8660fe30331a4a28b1019c7fe429dc9b6c1276e"
}

action "GitHub Action for Docker" {
  uses = "actions/docker/cli@86ab5e854a74b50b7ed798a94d9b8ce175d8ba19"
  runs = "ls"
  args = " -latr /"
}
