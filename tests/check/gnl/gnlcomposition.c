/* Gnonlin
 * Copyright (C) <2009> Alessandro Decina <alessandro.decina@collabora.co.uk>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "common.h"

typedef struct
{
  GstElement *composition;
  GstElement *source3;
} TestClosure;

static int composition_pad_added;
static int composition_pad_removed;
static int seek_events;

static gboolean
on_source1_pad_event_cb (GstPad * pad, GstEvent * event, gpointer user_data)
{
  if (event->type == GST_EVENT_SEEK)
    ++seek_events;

  return TRUE;
}

static void
on_source1_pad_added_cb (GstElement * source, GstPad * pad, gpointer user_data)
{
  gst_pad_add_event_probe (pad, G_CALLBACK (on_source1_pad_event_cb), NULL);
}

static void
on_composition_pad_added_cb (GstElement * composition, GstPad * pad,
    GstElement * sink)
{
  GstPad *s = gst_element_get_pad (sink, "sink");
  gst_pad_link (pad, s);
  ++composition_pad_added;
  gst_object_unref (s);
}

static void
on_composition_pad_removed_cb (GstElement * composition, GstPad * pad,
    GstElement * sink)
{
  ++composition_pad_removed;
}

GST_START_TEST (test_change_object_start_stop_in_current_stack)
{
  GstElement *pipeline;
  guint64 start, stop;
  gint64 duration;
  GstElement *comp, *source1, *def, *sink;
  GstBus *bus;
  GstMessage *message;
  gboolean carry_on;
  int seek_events_before;

  pipeline = gst_pipeline_new ("test_pipeline");
  comp =
      gst_element_factory_make_or_warn ("gnlcomposition", "test_composition");

  sink = gst_element_factory_make_or_warn ("fakesink", "sink");
  gst_bin_add_many (GST_BIN (pipeline), comp, sink, NULL);

  /* connect to pad-added */
  g_object_connect (comp, "signal::pad-added",
      on_composition_pad_added_cb, sink, NULL);
  g_object_connect (comp, "signal::pad-removed",
      on_composition_pad_removed_cb, NULL, NULL);

  /*
     source1
     Start : 0s
     Duration : 2s
     Priority : 2
   */

  source1 = videotest_gnl_src ("source1", 0, 2 * GST_SECOND, 2, 2);
  g_object_connect (source1, "signal::pad-added",
      on_source1_pad_added_cb, NULL, NULL);

  check_start_stop_duration (source1, 0, 2 * GST_SECOND, 2 * GST_SECOND);

  /*
     def (default source)
     Priority = G_MAXUINT32
   */
  def =
      videotest_gnl_src ("default", 0 * GST_SECOND, 0 * GST_SECOND, 2,
      G_MAXUINT32);

  ASSERT_OBJECT_REFCOUNT (source1, "source1", 1);
  ASSERT_OBJECT_REFCOUNT (def, "default", 1);

  /* Add source 1 */

  /* keep an extra ref to source1 as we remove it from the bin */
  gst_object_ref (source1);
  gst_bin_add (GST_BIN (comp), source1);
  check_start_stop_duration (comp, 0, 2 * GST_SECOND, 2 * GST_SECOND);

  /* Add default */

  gst_bin_add (GST_BIN (comp), def);
  check_start_stop_duration (comp, 0, 2 * GST_SECOND, 2 * GST_SECOND);

  bus = gst_element_get_bus (GST_ELEMENT (pipeline));

  GST_DEBUG ("Setting pipeline to PLAYING");
  ASSERT_OBJECT_REFCOUNT (source1, "source1", 2);

  fail_if (gst_element_set_state (GST_ELEMENT (pipeline),
          GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE);

  GST_DEBUG ("Let's poll the bus");

  carry_on = TRUE;
  while (carry_on) {
    message = gst_bus_poll (bus, GST_MESSAGE_ANY, GST_SECOND / 2);
    if (message) {
      switch (GST_MESSAGE_TYPE (message)) {
        case GST_MESSAGE_ASYNC_DONE:
        {
          carry_on = FALSE;
          GST_DEBUG ("Pipeline reached PAUSED, stopping polling");
          break;
        }
        case GST_MESSAGE_EOS:
        {
          GST_WARNING ("Saw EOS");

          fail_if (TRUE);
        }
        case GST_MESSAGE_ERROR:
        {
          GError *error;
          char *debug;

          gst_message_parse_error (message, &error, &debug);

          GST_WARNING ("Saw an ERROR %s: %s (%s)",
              gst_object_get_name (message->src), error->message, debug);

          g_error_free (error);
          fail_if (TRUE);
        }
        default:
          break;
      }
      gst_mini_object_unref (GST_MINI_OBJECT (message));
    }
  }

  fail_unless_equals_int (composition_pad_added, 1);
  fail_unless_equals_int (composition_pad_removed, 0);

  seek_events_before = seek_events;

  /* pipeline is paused at this point */

  /* move source1 out of the active segment */
  g_object_set (source1, "start", 4 * GST_SECOND, NULL);
  fail_unless (seek_events > seek_events_before);

  /* remove source1 from the composition, which will become empty and remove the
   * ghostpad */
  gst_bin_remove (GST_BIN (comp), source1);
  /* Since the element is still active (PAUSED), there might be internal
   * refcounts still taking place, therefore we check if it's between
   * 1 and 2.
   * If we were to set it to NULL, it would be guaranteed to be 1, but
   * it would then be racy for the checks below (when we re-add it) */
  ASSERT_OBJECT_REFCOUNT_BETWEEN (source1, "source1", 1, 2);

  fail_unless_equals_int (composition_pad_added, 1);
  fail_unless_equals_int (composition_pad_removed, 1);

  g_object_set (source1, "start", 0 * GST_SECOND, NULL);
  /* add the source again and check that the ghostpad is added again */
  gst_bin_add (GST_BIN (comp), source1);

  fail_unless_equals_int (composition_pad_added, 2);
  fail_unless_equals_int (composition_pad_removed, 1);

  seek_events_before = seek_events;

  g_object_set (source1, "duration", 1 * GST_SECOND, NULL);
  fail_unless (seek_events > seek_events_before);

  GST_DEBUG ("Setting pipeline to NULL");

  fail_if (gst_element_set_state (GST_ELEMENT (pipeline),
          GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE);
  gst_element_set_state (source1, GST_STATE_NULL);
  gst_object_unref (source1);

  GST_DEBUG ("Resetted pipeline to READY");

  ASSERT_OBJECT_REFCOUNT_BETWEEN (pipeline, "main pipeline", 1, 2);
  gst_object_unref (pipeline);
  ASSERT_OBJECT_REFCOUNT_BETWEEN (bus, "main bus", 1, 2);
  gst_object_unref (bus);
}

GST_END_TEST;

GST_START_TEST (test_remove_invalid_object)
{
  GstBin *composition;
  GstElement *source1, *source2;

  composition = GST_BIN (gst_element_factory_make ("gnlcomposition",
          "composition"));
  source1 = gst_element_factory_make ("gnlsource", "source1");
  source2 = gst_element_factory_make ("gnlsource", "source2");

  gst_bin_add (composition, source1);
  fail_if (gst_bin_remove (composition, source2));
  fail_unless (gst_bin_remove (composition, source1));

  gst_object_unref (composition);
}

GST_END_TEST;

static void
pad_block (GstPad * pad, gboolean blocked, gpointer user_data)
{
  GstPad *ghost;
  GstBin *bin;

  if (!blocked)
    return;

  bin = GST_BIN (user_data);

  ghost = gst_ghost_pad_new ("src", pad);
  gst_pad_set_active (ghost, TRUE);

  gst_element_add_pad (GST_ELEMENT (bin), ghost);

  gst_pad_set_blocked_async (pad, FALSE, pad_block, NULL);
}

static void
no_more_pads_test_cb (GObject * object, TestClosure * c)
{
  GstElement *source2 = GST_ELEMENT (object);

  GST_WARNING ("NO MORE PADS");
  gst_bin_add (GST_BIN (c->composition), c->source3);
}

GST_START_TEST (test_no_more_pads_race)
{
  GstElement *source1, *source2, *source3;
  GstBin *bin;
  GstElement *videotestsrc1, *videotestsrc2, *videotestsrc3;
  GstElement *operation;
  GstElement *composition;
  GstElement *videomixer, *fakesink;
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *message;
  GstPad *pad;
  TestClosure closure;

  /* We create a composition with an operation and three sources. The operation
   * contains a videomixer instance and the three sources are videotestsrc's.
   *
   * One of the sources, source2, contains videotestsrc inside a bin. Initially
   * the bin doesn't have a source pad. We do this to exercise the dynamic src
   * pad code path in gnlcomposition. We block on the videotestsrc srcpad and in
   * the pad block callback we ghost the pad and add the ghost to the parent
   * bin. This makes gnlsource emit no-more-pads, which is used by
   * gnlcomposition to link the source2:src pad to videomixer.
   *
   * We start with the composition containing operation and source1. We preroll
   * and then add source2. Source2 will do what described above and emit
   * no-more-pads. We connect to that no-more-pads and from there we add source3 to
   * the composition. Adding a new source will make gnlcomposition deactivate
   * the old stack and activate a new one. The new one contains operation,
   * source1, source2 and source3. Source2 was active in the old stack as well and
   * gnlcomposition is *still waiting* for no-more-pads to be emitted on it
   * (since the no-more-pads emission is now blocked in our test's no-more-pads
   * callback, calling gst_bin_add). In short, here, we're simulating a race between
   * no-more-pads and someone modifying the composition.
   *
   * Activating the new stack, gnlcomposition calls compare_relink_single_node,
   * which finds an existing source pad for source2 this time since we have
   * already blocked and ghosted. It takes another code path that assumes that
   * source2 doesn't have dynamic pads and *BOOM*.
   */

  pipeline = GST_ELEMENT (gst_pipeline_new (NULL));
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  composition = gst_element_factory_make ("gnlcomposition", "composition");
  fakesink = gst_element_factory_make ("fakesink", NULL);
  g_object_set (fakesink, "sync", TRUE, NULL);

  /* operation */
  operation = gst_element_factory_make ("gnloperation", "operation");
  videomixer = gst_element_factory_make ("videomixer", "videomixer");
  gst_bin_add (GST_BIN (operation), videomixer);
  g_object_set (operation, "start", 0 * GST_SECOND, "duration", 20 * GST_SECOND,
      "media-start", 0 * GST_SECOND, "media-duration", 20 * GST_SECOND,
      "priority", 10, NULL);
  gst_bin_add (GST_BIN (composition), operation);

  /* source 1 */
  source1 = gst_element_factory_make ("gnlsource", "source1");
  videotestsrc1 = gst_element_factory_make ("videotestsrc", "videotestsrc1");
  gst_bin_add (GST_BIN (source1), videotestsrc1);
  g_object_set (source1, "start", 0 * GST_SECOND, "duration", 10 * GST_SECOND,
      "media-start", 0 * GST_SECOND, "media-duration", 10 * GST_SECOND,
      "priority", 20, NULL);

  /* source2 */
  source2 = gst_element_factory_make ("gnlsource", "source2");
  bin = GST_BIN (gst_bin_new (NULL));
  videotestsrc2 = gst_element_factory_make ("videotestsrc", "videotestsrc2");
  pad = gst_element_get_static_pad (videotestsrc2, "src");
  gst_pad_set_blocked_async (pad, TRUE, pad_block, bin);
  gst_bin_add (bin, videotestsrc2);
  gst_bin_add (GST_BIN (source2), GST_ELEMENT (bin));
  g_object_set (source2, "start", 0 * GST_SECOND, "duration", 10 * GST_SECOND,
      "media-start", 0 * GST_SECOND, "media-duration", 10 * GST_SECOND,
      "priority", 20, NULL);

  /* source3 */
  source3 = gst_element_factory_make ("gnlsource", "source3");
  videotestsrc2 = gst_element_factory_make ("videotestsrc", "videotestsrc3");
  gst_bin_add (GST_BIN (source3), videotestsrc2);
  g_object_set (source3, "start", 0 * GST_SECOND, "duration", 10 * GST_SECOND,
      "media-start", 0 * GST_SECOND, "media-duration", 10 * GST_SECOND,
      "priority", 20, NULL);

  closure.composition = composition;
  closure.source3 = source3;
  g_object_connect (source2, "signal::no-more-pads",
      no_more_pads_test_cb, &closure, NULL);

  gst_bin_add (GST_BIN (composition), source1);
  g_object_connect (composition, "signal::pad-added",
      on_composition_pad_added_cb, fakesink, NULL);
  g_object_connect (composition, "signal::pad-removed",
      on_composition_pad_removed_cb, NULL, NULL);

  gst_bin_add_many (GST_BIN (pipeline), composition, fakesink, NULL);

  fail_if (gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PAUSED)
      == GST_STATE_CHANGE_FAILURE);

  message = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR);
  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
    GError *error;
    gchar *debug;

    gst_message_parse_error (message, &error, &debug);
    fail_if (TRUE, "error: %s - %s", error->message, debug);
  }
  gst_message_unref (message);

  /* FIXME: maybe slow down the videotestsrc steaming thread */
  gst_bin_add (GST_BIN (composition), source2);

  message = gst_bus_timed_pop_filtered (bus, 1 * GST_SECOND, GST_MESSAGE_ERROR);
  if (message) {
    if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR) {
      GError *error;
      gchar *debug;

      gst_message_parse_error (message, &error, &debug);
      fail_if (TRUE, error->message);
    } else {
      fail_if (TRUE);
    }

    gst_message_unref (message);
  }

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);
  gst_object_unref (pipeline);
  gst_object_unref (bus);
}

GST_END_TEST;

Suite *
gnonlin_suite (void)
{
  Suite *s = suite_create ("gnonlin");
  TCase *tc_chain = tcase_create ("gnlcomposition");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_change_object_start_stop_in_current_stack);
  tcase_add_test (tc_chain, test_remove_invalid_object);
  tcase_add_test (tc_chain, test_no_more_pads_race);

  return s;
}

int
main (int argc, char **argv)
{
  int nf;

  Suite *s = gnonlin_suite ();
  SRunner *sr = srunner_create (s);

  gst_check_init (&argc, &argv);

  srunner_run_all (sr, CK_NORMAL);
  nf = srunner_ntests_failed (sr);
  srunner_free (sr);

  return nf;
}
