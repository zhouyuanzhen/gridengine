/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2003 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "basis_types.h"
#include "sge_qmaster_threads.h"
#include "sgermon.h"
#include "sge_mt_init.h"
#include "sge_prog.h"
#include "sge_log.h"
#include "sge_unistd.h"
#include "sge_answer.h"
#include "setup_qmaster.h"
#include "sge_security.h"
#include "sge_manop.h"
#include "sge_mtutil.h"
#include "sge_lock.h"
#include "sge_qmaster_process_message.h"
#include "sge_event_master.h"
#include "sge_persistence_qmaster.h"
#include "sge_reporting_qmaster.h"
#include "sge_qmaster_timed_event.h"
#include "sge_host_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_give_jobs.h"
#include "sge_all_listsL.h"
#include "sge_calendar_qmaster.h"
#include "sge_time.h"
#include "lock.h"
#include "qmaster_heartbeat.h"
#include "shutdown.h"
#include "sge_spool.h"
#include "cl_commlib.h"
#include "sge_uidgid.h"
#include "sge_bootstrap.h"
#include "msg_common.h"
#include "msg_qmaster.h"
#include "msg_daemons_common.h"
#include "msg_utilib.h"  /* remove once 'sge_daemonize_qmaster' did become 'sge_daemonize' */
#include "sge.h"
#include "sge_qmod_qmaster.h"
#include "reschedule.h"
#include "sge_job_qmaster.h"
#include "sge_profiling.h"
#include "sgeobj/sge_conf.h"
#include "qm_name.h"
#include "setup_path.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_sched_process_events.h"
#include "sge_follow.h"

#include "gdi/sge_gdi_packet.h"

#include "uti/sge_os.h"
#include "uti/sge_thread_ctrl.h"

#include "sge_thread_main.h"
#include "sge_thread_timer.h"
#include "sge_thread_event_master.h"
#include "sge_qmaster_timed_event.h"
#include "sge_qmaster_heartbeat.h"
#include "sge_persistence_qmaster.h"

static void
sge_event_master_cleanup_monitor(monitoring_t *monitor)
{
   DENTER(TOP_LAYER, "sge_event_master_cleanup_monitor");
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

static void
sge_event_master_cleanup_report_list(lList **list)
{
   DENTER(TOP_LAYER, "sge_event_master_cleanup_monitor");
   lFreeList(list);
   DRETURN_VOID;
}

void 
sge_event_master_initialize(sge_gdi_ctx_class_t *ctx)
{
   cl_thread_settings_t* dummy_thread_p = NULL;
   dstring thread_name = DSTRING_INIT;

   DENTER(TOP_LAYER, "sge_event_master_initialize");

   DPRINTF(("event master functionality has been initialized\n"));

   sge_dstring_sprintf(&thread_name, "%s%03d", threadnames[DELIVERER_THREAD], 0);
   cl_thread_list_setup(&(Main_Control.event_master_thread_pool), "event master thread pool");
   cl_thread_list_create_thread(Main_Control.event_master_thread_pool, &dummy_thread_p,
                                NULL, sge_dstring_get_string(&thread_name), 0, 
                                sge_event_master_main, NULL, NULL);
   sge_dstring_free(&thread_name);
   DRETURN_VOID;
}

void
sge_event_master_terminate(void)
{
   cl_thread_settings_t* thread = NULL;

   DENTER(TOP_LAYER, "sge_event_master_terminate");

   thread = cl_thread_list_get_first_thread(Main_Control.event_master_thread_pool);
   while (thread != NULL) {
      DPRINTF((SFN" gets canceled\n", thread->thread_name));
      cl_thread_list_delete_thread(Main_Control.event_master_thread_pool, thread);
      thread = cl_thread_list_get_first_thread(Main_Control.event_master_thread_pool);
   }  
   DPRINTF(("all "SFN" threads terminated\n", threadnames[DELIVERER_THREAD]));

   sge_event_master_shutdown();

   DRETURN_VOID;
}

void *
sge_event_master_main(void *arg)
{
   bool do_endlessly = true;
   cl_thread_settings_t *thread_config = (cl_thread_settings_t*)arg;
   sge_gdi_ctx_class_t *ctx = NULL;
   monitoring_t monitor;

   lListElem *report = NULL;
   lList *report_list = NULL;
   time_t next_prof_output = 0;

   DENTER(TOP_LAYER, "sge_event_master_main");

   DPRINTF((SFN" started", thread_config->thread_name));
   cl_thread_func_startup(thread_config);
   sge_monitor_init(&monitor, thread_config->thread_name, TET_EXT, TET_WARNING, TET_ERROR);
   sge_qmaster_thread_init(&ctx, QMASTER, DELIVERER_THREAD, true);

   /* register at profiling module */
   set_thread_name(pthread_self(), "Deliver Thread");
   conf_update_thread_profiling("Deliver Thread");

   report_list = lCreateListHash("report list", REP_Type, false);
   report = lCreateElem(REP_Type);
   lSetUlong(report, REP_type, NUM_REP_REPORT_EVENTS);
   lSetHost(report, REP_host, ctx->get_qualified_hostname(ctx));
   lAppendElem(report_list, report);
 
   while (do_endlessly) {
      int execute = 0;

      thread_start_stop_profiling();

      sge_mutex_lock("event_master_cond_mutex", SGE_FUNC, __LINE__, 
                     &Event_Master_Control.cond_mutex);

#ifdef EVC_DEBUG
{   
dstring dsbuf;
char buf[1024];
sge_dstring_init(&dsbuf, buf, sizeof(buf));
printf("##### before sge_event_master_wait_next() at %s\n", sge_ctime(0, &dsbuf));
}
#endif
      /*
       * did a new event arrive which has a flush time of 0 seconds?
       */
      MONITOR_IDLE_TIME(sge_event_master_wait_next(), (&monitor), mconf_get_monitor_time(), 
                        mconf_is_monitor_message());

      sge_mutex_unlock("event_master_cond_mutex", SGE_FUNC, __LINE__, 
                       &Event_Master_Control.cond_mutex);

      MONITOR_MESSAGES((&monitor));
      MONITOR_EDT_COUNT((&monitor));
      /* If the client array has changed, rebuild the indices. */
      MONITOR_WAIT_TIME(sge_mutex_lock("event_master_mutex", SGE_FUNC, __LINE__,
                        &Event_Master_Control.mutex), (&monitor));
    
      MONITOR_CLIENT_COUNT((&monitor), lGetNumberOfElem(Event_Master_Control.clients));

      if (Event_Master_Control.indices_dirty) {
         lListElem *ep = NULL;
         int count = 0;

         DPRINTF(("Rebuilding indices\n"));

         /* For a large number of event clients, this loop would be faster as
          * a for loop that walks through the clients_array. */
         for_each(ep, Event_Master_Control.clients) {
            Event_Master_Control.clients_indices[count++] = (int)lGetUlong(ep, EV_id);
         }

         Event_Master_Control.clients_indices[count] = 0;
         Event_Master_Control.indices_dirty = false;
      }

      sge_mutex_unlock("event_master_mutex", SGE_FUNC, __LINE__, &Event_Master_Control.mutex);

      sge_event_master_process_mod_event_client(&monitor);
      sge_event_master_process_acks(&monitor);
      sge_event_master_process_sends(&monitor);
      sge_event_master_send_events(ctx, report, report_list, &monitor);
      sge_monitor_output(&monitor);

      thread_output_profiling("event master thread profiling summary:\n",
                              &next_prof_output);

#ifdef EVC_DEBUG
{   
dstring dsbuf;
char buf[1024];
sge_dstring_init(&dsbuf, buf, sizeof(buf));
printf("##### after processing event_master funcs at %s\n", sge_ctime(0, &dsbuf));
}
#endif
      /* pthread cancelation point */
      pthread_cleanup_push((void (*)(void *))sge_event_master_cleanup_monitor,
                           (void *)&monitor);
      pthread_cleanup_push((void (*)(void *))sge_event_master_cleanup_report_list,
                           (void *)&report_list);
      cl_thread_func_testcancel(thread_config);
      pthread_cleanup_pop(execute); 
      pthread_cleanup_pop(execute); 
      if (sge_thread_has_shutdown_started()) {
         DPRINTF((SFN" is waiting for termination\n", thread_config->thread_name));
         sleep(1);
      }
   }

   /*
    * Don't add cleanup code here. It will never be executed. Instead register
    * a cleanup function with pthread_cleanup_push()/pthread_cleanup_pop() before 
    * and after the call of cl_thread_func_testcancel()
    */

   DRETURN(NULL);
}
