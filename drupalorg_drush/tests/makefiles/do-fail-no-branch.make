core = 7.10
api = 2

; Should fail since neither project-level 'version' nor download-level
; 'branch' is specified.
projects[update_test_module][download][type] = "git"
projects[update_test_module][download][revision] = "2bfc8d9"

