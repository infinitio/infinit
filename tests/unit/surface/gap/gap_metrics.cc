#include <surface/gap/gap.h>

int
main(void)
{
  gap_State* state = gap_new();

  gap_metrics_google_connect_attempt(state);
  gap_metrics_google_connect_succeed(state);
  gap_metrics_google_connect_fail(state);
  gap_metrics_facebook_connect_attempt(state);
  gap_metrics_facebook_connect_succeed(state);
  gap_metrics_facebook_connect_fail(state);
  gap_metrics_google_login_attempt(state);
  gap_metrics_google_login_succeed(state);
  gap_metrics_google_login_fail(state);
  gap_metrics_facebook_login_attempt(state);
  gap_metrics_facebook_login_succeed(state);
  gap_metrics_facebook_login_fail(state);
  gap_metrics_google_import_attempt(state);
  gap_metrics_google_import_succeed(state);
  gap_metrics_google_import_fail(state);
  gap_metrics_facebook_import_attempt(state);
  gap_metrics_facebook_import_succeed(state);
  gap_metrics_facebook_import_fail(state);
  gap_metrics_google_invite_attempt(state);
  gap_metrics_google_invite_succeed(state);
  gap_metrics_google_invite_fail(state);
  gap_metrics_facebook_invite_attempt(state);
  gap_metrics_facebook_invite_succeed(state);
  gap_metrics_facebook_invite_fail(state);
  gap_metrics_google_share_attempt(state);
  gap_metrics_google_share_succeed(state);
  gap_metrics_google_share_fail(state);
  gap_metrics_drop_self(state);
  gap_metrics_drop_favorite(state);
  gap_metrics_drop_bar(state);
  gap_metrics_drop_user(state);
  gap_metrics_drop_nowhere(state);
  gap_metrics_click_self(state);
  gap_metrics_click_favorite(state);
  gap_metrics_click_searchbar(state);
  gap_metrics_transfer_self(state);
  gap_metrics_transfer_favorite(state);
  gap_metrics_transfer_user(state);
  gap_metrics_transfer_social(state);
  gap_metrics_transfer_email(state);
  gap_metrics_transfer_ghost(state);
  gap_metrics_panel_open(state, "panel");
  gap_metrics_panel_close(state, "panel");
  gap_metrics_panel_accept(state, "panel");
  gap_metrics_panel_deny(state, "panel");
  gap_metrics_panel_access(state, "panel");
  gap_metrics_panel_cancel(state, "panel", "author");
  gap_metrics_dropzone_open(state);
  gap_metrics_dropzone_close(state);
  gap_metrics_dropzone_removeitem(state);
  gap_metrics_dropzone_removeall(state);
  gap_metrics_search(state, "input");
  gap_metrics_searchbar_search(state);
  gap_metrics_searchbar_invite(state, "input");
  gap_metrics_searchbar_share(state, "input");
  gap_metrics_select_user(state, "input");
  gap_metrics_select_social(state, "input");
  gap_metrics_select_ghost(state, "input");
  gap_metrics_select_close(state, "input");

  gap_free(state);

  return 0;
}
