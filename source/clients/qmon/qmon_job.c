/*___INFO__MARK_BEGIN__*/
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
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/
#define QALTER_RUNNING


#include <stdio.h>
#include <stdlib.h>
#include <Xm/ToggleB.h>

#include <Xmt/Xmt.h>
#include <Xmt/Create.h>
#include <Xmt/Icon.h>
#include <Xmt/Dialogs.h>
#include <Xmt/Chooser.h>
#include <Xmt/InputField.h>

#include "Matrix.h"
#include "Tab.h"
#include "qmon_rmon.h"
#include "qmon_cull.h"
#include "qmon_job.h"
#include "qmon_comm.h"
#include "qmon_jobcustom.h"
#include "qmon_appres.h"
#include "qmon_submit.h"
#include "qmon_timer.h"
#include "qmon_globals.h"
#include "qmon_browser.h"
#include "qmon_message.h"
#include "qmon_init.h"
#include "qmon_ticket.h"
#include "qmon_request.h"
#include "qmon_util.h"
#include "basis_types.h"
#include "def.h"
#include "sge_sched.h"
#include "sge_signal.h"
#include "sge_time.h"
#include "sge_job_schedd.h"
#include "sge_resource.h"
#include "sge_all_listsL.h"
#include "gdi_qmod.h"
#include "sge_gdi_intern.h"
#include "sge_feature.h"
#include "parse_range.h"
#include "qmon_matrix.h"
#include "qstat_util.h"
#include "parse.h"
#include "job.h"

enum {
   JOB_DISPLAY_MODE_RUNNING,
   JOB_DISPLAY_MODE_PENDING
};

typedef struct _tAskHoldInfo {
   Boolean blocked;
   XmtButtonType button;
   Widget AskHoldDialog;
   Widget AskHoldFlags;
   Widget AskHoldTasks;
} tAskHoldInfo;
   

static Widget qmon_job = 0;
static Widget job_running_jobs = 0;
static Widget job_pending_jobs = 0;
static Widget job_zombie_jobs = 0;
static Widget job_customize = 0;
static Widget job_schedd_info = 0;
static Widget job_delete = 0;
static Widget job_reschedule = 0;
static Widget job_qalter = 0;
static Widget job_suspend = 0;
static Widget job_unsuspend = 0;
static Widget job_hold = 0;
static Widget job_error = 0;
static Widget job_priority = 0;
static Widget force_toggle = 0;


/*-------------------------------------------------------------------------*/
static void qmonCreateJobControl(Widget parent);
static void qmonJobToMatrix(Widget w, lListElem *jep, lListElem *jat, lList *jal, int mode, int row);
/* static void qmonSetMatrixLabels(Widget w, lDescr *dp); */
static void qmonJobFolderChange(Widget w, XtPointer cld, XtPointer cad);
static void qmonDeleteJobCB(Widget w, XtPointer cld, XtPointer cad);
static void qmonJobPopdown(Widget w, XtPointer cld, XtPointer cad);
static void qmonJobStartUpdate(Widget w, XtPointer cld, XtPointer cad);
static void qmonJobStopUpdate(Widget w, XtPointer cld, XtPointer cad);
static void qmonJobHandleEnterLeave(Widget w, XtPointer cld, XEvent *ev, Boolean *ctd);
static String qmonJobShowBrowserInfo(lListElem *jep);
static void qmonJobChangeState(Widget w, XtPointer cld, XtPointer cad);
static void qmonJobPriority(Widget w, XtPointer cld, XtPointer cad);
static void qmonJobHold(Widget w, XtPointer cld, XtPointer cad);
static void qmonJobQalter(Widget w, XtPointer cld, XtPointer cad);
static void qmonJobNoEdit(Widget w, XtPointer cld, XtPointer cad);
static Pixel qmonJobStateToColor(Widget w, lListElem *jep);
static Boolean qmonDeleteJobForMatrix(Widget w, Widget matrix, lList **local);
static lList* qmonJobBuildSelectedList(Widget matrix, lDescr *dp, int nm);
/* static void qmonResizeCB(Widget w, XtPointer cld, XtPointer cad); */
static Boolean AskForHold(Widget w, Cardinal *hold, StringBufferT *dyn_tasks);
static void AskHoldCancelCB(Widget w, XtPointer cld, XtPointer cad);
static void AskHoldOkCB(Widget w, XtPointer cld, XtPointer cad);
static void qmonJobScheddInfo(Widget w, XtPointer cld, XtPointer cad);
/*-------------------------------------------------------------------------*/


/*-------------------------------------------------------------------------*/
/* P U B L I C                                                             */
/*-------------------------------------------------------------------------*/
void qmonJobPopup(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
   
   DENTER(GUI_LAYER, "qmonJobPopup");

   /* set busy cursor */
   XmtDisplayBusyCursor(w);

   if (!qmon_job) {
      qmonMirrorMulti(JOB_T | QUEUE_T | COMPLEX_T | EXECHOST_T | ZOMBIE_T);
      qmonCreateJobControl(AppShell);
      /*
      ** create the job customize dialog
      */
      qmonCreateJCU(qmon_job, NULL);
      /* 
      ** set the close button callback 
      ** set the icon and iconName
      */
      XmtCreatePixmapIcon(qmon_job, qmonGetIcon("toolbar_job"), None);
      XtVaSetValues(qmon_job, XtNiconName, "qmon:Job Control", NULL);
      XmtAddDeleteCallback(qmon_job, XmDO_NOTHING, qmonJobPopdown,  NULL);
      XtAddEventHandler(qmon_job, StructureNotifyMask, False, 
                        SetMinShellSize, NULL);
      XtAddEventHandler(qmon_job, StructureNotifyMask, False, 
                        SetMaxShellSize, NULL);
   }

   /* updateJobListCB(qmon_job, NULL, NULL); */

   xmui_manage(qmon_job);

   /* set normal cursor */
   XmtDisplayDefaultCursor(w);

   DEXIT;
}


/*-------------------------------------------------------------------------*/
void qmonJobReturnMatrix(
Widget *run_m,
Widget *pen_m,
Widget *zombie_m 
) {
   DENTER(GUI_LAYER, "qmonJobReturnMatrix");

   *run_m = job_running_jobs;
   *pen_m = job_pending_jobs;
   *zombie_m = job_zombie_jobs;

   DEXIT;
}

/*-------------------------------------------------------------------------*/
void updateJobListCB(w, cld, cad) 
Widget w;
XtPointer cld, cad;
{
   /* set busy cursor */
   XmtDisplayBusyCursor(w);

   if (qmonMirrorMulti(JOB_T|QUEUE_T|COMPLEX_T|EXECHOST_T|ZOMBIE_T|USERSET_T|PROJECT_T) != -1)
      updateJobList();

   /* set busy cursor */
   XmtDisplayDefaultCursor(w);
}


/*-------------------------------------------------------------------------*/
/* P R I V A T E                                                           */
/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
static void qmonJobPopdown(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
   DENTER(GUI_LAYER, "qmonJobPopdown");

   xmui_unmanage(qmon_job);
   
   DEXIT;
}

/*-------------------------------------------------------------------------*/
static void qmonJobStartUpdate(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
   DENTER(GUI_LAYER, "qmonJobStartUpdate");
  
   qmonTimerAddUpdateProc(JOB_T|ZOMBIE_T, "updateJobList", updateJobList);
   qmonStartTimer(JOB_T | QUEUE_T | COMPLEX_T | EXECHOST_T | ZOMBIE_T);
   
   DEXIT;
}


/*-------------------------------------------------------------------------*/
static void qmonJobStopUpdate(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
   DENTER(GUI_LAYER, "qmonJobStopUpdate");
  
   qmonStopTimer(JOB_T | QUEUE_T | COMPLEX_T | EXECHOST_T | ZOMBIE_T);
   qmonTimerRmUpdateProc(JOB_T|ZOMBIE_T, "updateJobList");
   
   DEXIT;
}


/*-------------------------------------------------------------------------*/
static void qmonCreateJobControl(
Widget parent 
) {
   Widget job_submit, job_update, job_done, job_tickets,
          job_main_link, job_pending, job_folder;
   static Widget pw[2];
   static Widget rw[2];
   
   DENTER(GUI_LAYER, "qmonCreateJobControl");

   qmon_job = XmtBuildQueryToplevel( parent, 
                                     "qmon_job",
                                     "job_running_jobs", &job_running_jobs,
                                     "job_pending_jobs", &job_pending_jobs,
                                     "job_zombie_jobs", &job_zombie_jobs,
                                     "job_delete", &job_delete,
                                     "job_error", &job_error,
                                     "job_qalter", &job_qalter,
                                     "job_priority", &job_priority,
                                     "job_update", &job_update,
                                     "job_suspend", &job_suspend,
                                     "job_reschedule", &job_reschedule,
                                     "job_unsuspend", &job_unsuspend,
                                     "job_customize", &job_customize,
                                     "job_submit", &job_submit,
                                     "job_tickets", &job_tickets,
                                     "job_done", &job_done,
                                     "job_hold", &job_hold,
                                     "job_schedd_info", &job_schedd_info,
                                     "job_main_link", &job_main_link,
                                     "job_folder", &job_folder,
                                     "job_pending", &job_pending,
                                     "job_force",  &force_toggle,
                                     NULL);
   pw[0] = job_running_jobs;
   pw[1] = NULL;
   rw[0] = job_pending_jobs;
   rw[1] = NULL;


   if (!feature_is_enabled(FEATURE_SGEEE)) {
      XtUnmanageChild(job_tickets);
   }
   else {
      XtAddCallback(job_tickets, XmNactivateCallback,
                     qmonPopupTicketOverview, NULL);
   }

   /* start the needed timers and the corresponding update routines */
   XtAddCallback(qmon_job, XmNpopupCallback, 
                     qmonJobStartUpdate, NULL);
   XtAddCallback(qmon_job, XmNpopdownCallback,
                     qmonJobStopUpdate, NULL);
                     
   XtAddCallback(job_folder, XmNvalueChangedCallback, 
                     qmonJobFolderChange, NULL);
   XmTabSetTabWidget(job_folder, job_pending, True);

   XtAddCallback(job_delete, XmNactivateCallback, 
                     qmonDeleteJobCB, NULL);
   XtAddCallback(job_update, XmNactivateCallback, 
                     updateJobListCB, NULL);
   XtAddCallback(job_customize, XmNactivateCallback,  
                     qmonPopupJCU, NULL); 
   XtAddCallback(job_submit, XmNactivateCallback, 
                     qmonSubmitPopup, NULL);
   XtAddCallback(job_priority, XmNactivateCallback, 
                     qmonJobPriority, NULL);
   XtAddCallback(job_hold, XmNactivateCallback, 
                     qmonJobHold, NULL);
   XtAddCallback(job_schedd_info, XmNactivateCallback, 
                     qmonJobScheddInfo, NULL);
   XtAddCallback(job_qalter, XmNactivateCallback, 
                     qmonJobQalter, NULL);
   XtAddCallback(job_done, XmNactivateCallback, 
                     qmonJobPopdown, NULL);
   XtAddCallback(job_main_link, XmNactivateCallback, 
                     qmonMainControlRaise, NULL);
/*    XtAddCallback(job_running_jobs, XmNresizeColumnCallback,  */
/*                      qmonResizeCB, NULL); */
   XtAddCallback(job_running_jobs, XmNenterCellCallback, 
                     qmonJobNoEdit, NULL);
   XtAddCallback(job_pending_jobs, XmNenterCellCallback, 
                     qmonJobNoEdit, NULL);
   XtAddCallback(job_running_jobs, XmNselectCellCallback, 
                     qmonMatrixSelect, (XtPointer) rw);
   XtAddCallback(job_pending_jobs, XmNselectCellCallback, 
                     qmonMatrixSelect, (XtPointer) pw);
   XtAddCallback(job_suspend, XmNactivateCallback, 
                     qmonJobChangeState, (XtPointer)QSUSPENDED);
   XtAddCallback(job_unsuspend, XmNactivateCallback, 
                     qmonJobChangeState, (XtPointer)QRUNNING);
   XtAddCallback(job_error, XmNactivateCallback, 
                     qmonJobChangeState, (XtPointer)QERROR);
   XtAddCallback(job_reschedule, XmNactivateCallback, 
                     qmonJobChangeState, (XtPointer)QRESCHEDULED);

   /* Event Handler to display additional job info */
   XtAddEventHandler(job_running_jobs, PointerMotionMask, 
                     False, qmonJobHandleEnterLeave,
                     NULL); 
   XtAddEventHandler(job_pending_jobs, PointerMotionMask, 
                     False, qmonJobHandleEnterLeave,
                     NULL); 

   DEXIT;
}


/*-------------------------------------------------------------------------*/
static void qmonJobToMatrix(
Widget w,
lListElem *job,
lListElem *jat,
lList *tasks,
int mode,
int row                                                                       
) {
   int rows, columns;
   int col;
   String *rowa = NULL; 
   Pixel color;
   
   DENTER(GUI_LAYER, "qmonJobToMatrix");

   rows = XbaeMatrixNumRows(w);
   columns = XbaeMatrixNumColumns(w);

   /*
   ** running jobs
   */
   if (mode == JOB_DISPLAY_MODE_RUNNING) {
      rowa = PrintJobField(job, jat, NULL, columns);
      if (row < rows)
         XbaeMatrixDeleteRows(w, row, 1);
      XbaeMatrixAddRows(w, row, rowa, NULL, NULL, 1);
      /*
      ** do show the different job states we show the job id in a different           ** color
      */
      color = qmonJobStateToColor(w, jat);
      XbaeMatrixSetRowColors(w, row, &color, 1);
   }

   /*
   ** pending jobs
   */
   if (mode == JOB_DISPLAY_MODE_PENDING) {
      if (row < rows)
         XbaeMatrixDeleteRows(w, row, 1);
      rowa = PrintJobField(job, jat, tasks, columns);
      XbaeMatrixAddRows(w, row, rowa, NULL, NULL, 1);
      /*
      ** do show the different job states we show the job id in a different           ** color
      */
      if (!jat)
         jat = lFirst(tasks);
      color = qmonJobStateToColor(w, jat);
      XbaeMatrixSetRowColors(w, row, &color, 1);
   }

   /*
   ** free row array
   */
   if (rowa) {
      for (col=0; col<columns; col++)
         XtFree((char*)rowa[col]);
      XtFree((char*)rowa);
   }
                      
   DEXIT;
}

/*-------------------------------------------------------------------------*/
static Pixel qmonJobStateToColor(
Widget w,
lListElem *jat 
) {
   Pixel color;
   static Pixel fg = 0;
   int state;
   int hold;

   DENTER(GUI_LAYER, "qmonJobStateToColor");

   if (!fg)
      XtVaGetValues(w, XmNforeground, &fg, NULL);

   if (!jat) {
      DEXIT;
      return fg;
   }
      

   state = (int)lGetUlong(jat, JAT_state);
   hold = (int)lGetUlong(jat, JAT_hold);

   /*
   ** the order is important
   */

   color = fg;

   if (state & JSUSPENDED_ON_SUBORDINATE)
      color = JobSosPixel;

   if (state & JSUSPENDED)
      color = JobSuspPixel;

   if (state & JDELETED)
      color = JobDelPixel;

   if (hold)
      color = JobHoldPixel;

   if (state & JERROR)
      color = JobErrPixel;

   DEXIT;
   return color;

}

/*-------------------------------------------------------------------------*/
static void qmonJobNoEdit(
Widget w,
XtPointer cld,
XtPointer cad  
) {
   XbaeMatrixEnterCellCallbackStruct *cbs =
         (XbaeMatrixEnterCellCallbackStruct*) cad;

   DENTER(GUI_LAYER, "qmonJobNoEdit");

   cbs->doit = False;
   
   DEXIT;
}

/*----------------------------------------------------------------------------*/
void updateJobList(void)
{
   lList *jl = NULL;
   lList *ql = NULL;
   lListElem *jep = NULL;
   lListElem *qep = NULL;
   lCondition *where_run = NULL;
   lCondition *where_unfinished = NULL;
   lEnumeration *what = NULL;
   lCondition *where_no_template = NULL;
   lCondition *where_notexiting = NULL;
   lEnumeration *what_queue = NULL;
   lSortOrder *job_so = NULL;
   u_long32 jstate, qstate;
   String qnm;
   lList *ehl = NULL;
   lList *cl = NULL;
   lList *ol = NULL;
   lList *rl = NULL;
   lList *zl = NULL;
   static Boolean filter_on = False;
   int row = 0, pow = 0, zow = 0;
         
   DENTER(GUI_LAYER, "updateJobList");
   
   /*
   ** has dialog been created
   */
   if (!qmon_job) {
      DEXIT;
      return;
   }
   
   /*
   ** display only the unfinished jobs
   */
   where_unfinished = lWhere("%T(!(%I->(%I m= %u)))", 
                                 JB_Type, JB_ja_tasks, JAT_status, JFINISHED);
   what = lWhat("%T(ALL)", JB_Type);
   where_no_template = lWhere("%T(%I != %s)", QU_Type, QU_qname, 
                                    "template");
   what_queue = lWhat("%T(ALL)", QU_Type);
   where_notexiting = lWhere("%T(!(%I m= %u))", JAT_Type, JAT_status, JFINISHED);
   where_run = lWhere("%T((%I m= %u || %I m= %u) && (!(%I m= %u)))", 
                        JAT_Type, JAT_status, JRUNNING, JAT_status, JTRANSITING,
                        JAT_state, JEXITING);
 
   jl = lSelect("jl", qmonMirrorList(SGE_JOB_LIST), where_unfinished, what);

   ql = lSelect("ql", qmonMirrorList(SGE_QUEUE_LIST), where_no_template, 
                  what_queue);
   ehl = qmonMirrorList(SGE_EXECHOST_LIST);
   cl = qmonMirrorList(SGE_COMPLEX_LIST);
   /* don't free rl, ol they are maintained in qmon_jobcustom.c */
   rl = qmonJobFilterResources();
   ol = qmonJobFilterOwners();


   if (rl || ol) {
      if (!filter_on) {
         setButtonLabel(job_customize, "Customize *");
         filter_on = True;
      }
   }
   else {
      if (filter_on) {
         setButtonLabel(job_customize, "Customize");
         filter_on = False;
      }
   }
   
   /*
   ** match queues to request_list
   */
   if (rl) {
      lList *rel;
      lListElem *re;

      /* 
      ** put rl in a RE_Type list 
      */
      rel = lCreateList("RE_list", RE_Type);
      re = lCreateElem(RE_Type);
      lSetList(re, RE_entries, lCopyList("copied job filter request", rl));
      lAppendElem(rel, re);

      match_queue(&ql, rel, cl, ehl);
      rel = lFreeList(rel);
   }   
#if 0      
   {
      lListElem* ep;
      printf("Queues\n");
      for_each(ep, ql) {
         printf("Q: %s\n", lGetString(ep, QU_qname));
      }
   }
   {
      lListElem* ep;
      printf("All Jobs\n");
      for_each(ep, jl) {
         printf("J: %d\n", (int)lGetUlong(ep, JB_job_number));
      }
   }
#endif

   match_job(&jl, ol, ql, cl, ehl, rl); 

#if 0      
   {
      lListElem* ep;
      printf("Matched Jobs\n");
      for_each(ep, jl) {
         printf("J: %d\n", (int)lGetUlong(ep, JB_job_number));
      }
   }
#endif

   /*
   ** we need the ql no longer, free it
   */
   ql = lFreeList(ql);

   /*
   ** sort the jobs according to priority
   */
   if (lGetNumberOfElem(jl)>0 ) {
      if ((job_so = sge_job_sort_order(lGetListDescr(jl)))) {
         lSortList(jl, job_so);
         lFreeSortOrder(job_so);
      }
   }
 
   /*
   ** loop over the jobs and the tasks
   */
   XbaeMatrixDisableRedisplay(job_running_jobs);
   XbaeMatrixDisableRedisplay(job_pending_jobs);
   XbaeMatrixDisableRedisplay(job_zombie_jobs);
 
   /*
   ** reset matrices
   */
   XtVaSetValues( job_running_jobs,
                  XmNcells, NULL,
                  NULL);
   XtVaSetValues( job_pending_jobs,
                  XmNcells, NULL,
                  NULL);
   /*
   ** update the job entries
   */
   for_each (jep, jl) {
      lList *rtasks = lCopyList("rtasks", lGetList(jep, JB_ja_tasks));
      lList *ptasks = NULL;
      lListElem *jap;
      /*
      ** split into running and pending tasks
      */
      lSplit(&rtasks, &ptasks, "rtasks", where_run);
/* printf("========> where_run\n"); */
/* lWriteWhereTo(where_run, stdout); */
/* printf("========> rtasks\n"); */
/* lWriteListTo(rtasks, stdout); */
/* printf("========> ptasks\n"); */
/* lWriteListTo(ptasks, stdout); */
      /*
      ** for running tasks we have to set the suspend_on_subordinate flag
      ** if the granted queues are suspended the jat_state field must set
      ** the jsuspended_on_subordinate
      */
      if (lGetNumberOfElem(rtasks) > 0) {
         int tow = 0;
         for_each(jap, rtasks) {
            ql = lGetList(jap, JAT_granted_destin_identifier_list);
            if (ql) {
               lList *ehl = qmonMirrorList(SGE_EXECHOST_LIST);
               lList *cl = qmonMirrorList(SGE_COMPLEX_LIST);
               qnm = lGetString(lFirst(ql), JG_qname);
               qep = lGetElemStr(qmonMirrorList(SGE_QUEUE_LIST), QU_qname, qnm);
               if (qep) {
      /*             lList *lt = lGetList(qep, QU_load_thresholds); */
                  lList *st = lGetList(qep, QU_suspend_thresholds);
                  qstate = lGetUlong(qep, QU_state);
                  if (/* sge_load_alarm(qep, lt, ehl, cl, NULL) || */
                           sge_load_alarm(qep, st, ehl, cl, NULL)) {
                     jstate = lGetUlong(jap, JAT_state);
                     jstate &= ~JRUNNING; /* unset bit jrunning */
                     /* set bit jsuspended_on_subordinate */
                     jstate |= JSUSPENDED_ON_SUBORDINATE;
                     lSetUlong(jap, JAT_state, jstate);
                  }
               }
            }
            qmonJobToMatrix(job_running_jobs, jep, jap, NULL,
                              JOB_DISPLAY_MODE_RUNNING, row + tow);
            tow++;                                                            
         }
         rtasks = lFreeList(rtasks);
         row++;
      }
      /*
      ** pending jobs
      */
      if (lGetNumberOfElem(ptasks) > 0) {
         if (qmonJobFilterArraysCompressed()) {
            lList *task_group;
            lSplit(&ptasks, NULL, NULL, where_notexiting);
            while (( task_group = split_task_group(&ptasks))) {
               qmonJobToMatrix(job_pending_jobs, jep, NULL, task_group,
                              JOB_DISPLAY_MODE_PENDING, pow);
               task_group = lFreeList(task_group);
               pow++;
            }
         }
         else {
            int tow = 0;
            for_each(jap, ptasks) {
               qmonJobToMatrix(job_pending_jobs, jep, jap, NULL,
                                 JOB_DISPLAY_MODE_RUNNING, pow + tow);
               tow++;
            }
            pow++;
         }
         ptasks = lFreeList(ptasks);
      }
   }

   /*
   ** free the where/what
   */
   where_run = lFreeWhere(where_run);
   where_notexiting = lFreeWhere(where_notexiting);
   where_unfinished = lFreeWhere(where_unfinished);
   what = lFreeWhat(what);
   what_queue = lFreeWhat(what_queue);
   jl = lFreeList(jl);

   /*
   ** update the zombie job entries
   */
   zl = qmonMirrorList(SGE_ZOMBIE_LIST);

/* lWriteListTo(zl, stdout); */

   XtVaSetValues( job_zombie_jobs,
                  XmNcells, NULL,
                  NULL);

   for_each (jep, zl) {
      lListElem *jap;
      lList *ztasks = lCopyList("", lGetList(jep, JB_ja_tasks));
      if (lGetNumberOfElem(ztasks) > 0) {
         if (ztasks && qmonJobFilterArraysCompressed()) {
            lList *task_group;
            while (( task_group = split_task_group(&ztasks))) {
               qmonJobToMatrix(job_zombie_jobs, jep, NULL, task_group,
                              JOB_DISPLAY_MODE_PENDING, zow);
               task_group = lFreeList(task_group);
               zow++;
            }
         }
         else {
            int tow = 0;
            for_each(jap, ztasks) {
               qmonJobToMatrix(job_zombie_jobs, jep, jap, NULL,
                                 JOB_DISPLAY_MODE_PENDING, zow + tow);
               tow++;
            }
            zow++;
         }
         ztasks = lFreeList(ztasks);
      }
   }
 
   XbaeMatrixEnableRedisplay(job_running_jobs, True);
   XbaeMatrixEnableRedisplay(job_pending_jobs, True);
   XbaeMatrixEnableRedisplay(job_zombie_jobs, True);
 
   DEXIT;
}

/*-------------------------------------------------------------------------*/
static Boolean qmonDeleteJobForMatrix(
Widget w,
Widget matrix,
lList **local 
) {
   int i;
   int rows;
   lList *jl = NULL;
   lListElem *jep = NULL;
   lList *alp = NULL;
   char *str;
   int force;

   DENTER(GUI_LAYER, "qmonDeleteJobForMatrix");

   force = XmToggleButtonGetState(force_toggle);

   /* 
   ** create delete job list 
   */
   rows = XbaeMatrixNumRows(matrix);
   for (i=0; i<rows; i++) {
      /* is this row selected */ 
      if (XbaeMatrixIsRowSelected(matrix, i)) {
         str = XbaeMatrixGetCell(matrix, i, 0);
         if ( str && *str != '\0' ) { 
            DPRINTF(("JobTask(s) to delete: %s\n", str));
            if (sge_parse_jobtasks(&jl, &jep, str, &alp) == -1) {
               qmonMessageBox(w, alp, 0);
               DEXIT;
               return False;
            }
            lSetUlong(jep, ID_force, force);
         }
      }
   }

   if (jl) {
      alp = qmonDelList(SGE_JOB_LIST, local, 
                        ID_str, &jl, NULL, NULL);

   
      qmonMessageBox(w, alp, 0);

      updateJobList();
      XbaeMatrixDeselectAll(matrix);

      jl = lFreeList(jl);
      alp = lFreeList(alp);
   } 
#if 0   
   else {
      qmonMessageShow(w, True, "@{There are no jobs selected !}");
      DEXIT;
      return False;
   }
#endif

   DEXIT;
   return True;
}


/*-------------------------------------------------------------------------*/
static void qmonDeleteJobCB(
Widget w,
XtPointer cld,
XtPointer cad 
) {
   Boolean status;
   
   DENTER(GUI_LAYER, "qmonDeleteJobCB");

   /*
   ** we have to delete the running jobs independently
   ** they are not removed from the local list only their
   ** state is set to JDELETED, this is marked with a NULL pointer
   ** instead of a valid local list address
   */

   /* set busy cursor */
   XmtDisplayBusyCursor(w);

   status = qmonDeleteJobForMatrix(w, job_running_jobs, NULL);
   if (status)
      XbaeMatrixDeselectAll(job_running_jobs);

   status = qmonDeleteJobForMatrix(w, job_pending_jobs,
                           qmonMirrorListRef(SGE_JOB_LIST));
   if (status)
      XbaeMatrixDeselectAll(job_pending_jobs);

   updateJobListCB(w, NULL, NULL);

   /* set normal cursor */
   XmtDisplayDefaultCursor(w);

   DEXIT;
}

/*-------------------------------------------------------------------------*/
static void qmonJobChangeState(
Widget w,
XtPointer cld,
XtPointer cad 
) {
   lList *jl = NULL;
   lList *rl = NULL;
   lList *alp = NULL;
   int force = 0;
   int action = (int)(long)cld;
   Widget force_toggle;
   
   DENTER(GUI_LAYER, "qmonJobChangeState");

   if (action == QERROR) {
      rl = qmonJobBuildSelectedList(job_running_jobs, ST_Type, STR);
      
      jl = qmonJobBuildSelectedList(job_pending_jobs, ST_Type, STR);

      if (!jl && rl) {
         jl = rl;
      }
      else if (rl) {
         lAddList(jl, rl);
      }
   }
   else {
      force_toggle = XmtNameToWidget(w, "*job_force");

      force = XmToggleButtonGetState(force_toggle);
      /*
      ** state changes only for running jobs
      */
      jl = qmonJobBuildSelectedList(job_running_jobs, ST_Type, STR);
   }

   if (jl) {

      alp = qmonChangeStateList(SGE_JOB_LIST, jl, force, action); 

      qmonMessageBox(w, alp, 0);

      updateJobList();
      XbaeMatrixDeselectAll(job_running_jobs);
      XbaeMatrixDeselectAll(job_pending_jobs);

      jl = lFreeList(jl);
      alp = lFreeList(alp);
   }
   else {
      qmonMessageShow(w, True, "@{There are no jobs selected !}");
   }
   
   DEXIT;
}

/*-------------------------------------------------------------------------*/
/* descriptor must contain JB_job_number at least                          */
/*-------------------------------------------------------------------------*/
static lList* qmonJobBuildSelectedList(matrix, dp, nm)
Widget matrix;
lDescr *dp;
{
   int i;
   int rows;
   lList *jl = NULL;
   lListElem *jep = NULL;
   String str;

   DENTER(GUI_LAYER, "qmonJobBuildSelectedList");

   if (nm != JB_job_number && nm != STR) {
      DEXIT;
      return NULL;
   }

   rows = XbaeMatrixNumRows(matrix);
      
   for (i=0; i<rows; i++) {
      /* is this row selected */ 
      if (XbaeMatrixIsRowSelected(matrix, i)) {
         str = XbaeMatrixGetCell(matrix, i, 0);
         DPRINTF(("Job to alter: %s\n", str));
         /*
         ** list with describtion job id's and the priority value 
         */
         if ( str && (*str != '\0') ) { 
            switch (nm) { 
               case JB_job_number:
                  {
                     u_long32 start = 0, end = 0, step = 0;
                     lListElem *idp = NULL;
                     lList *ipp = NULL;
                     lList *jat_list = NULL;
                     lList *alp = NULL;
                     if (sge_parse_jobtasks(&ipp, &idp, str, &alp) == -1) {
                        alp = lFreeList(alp);
                        DEXIT;
                        return NULL;
                     }
                     alp = lFreeList(alp);

                     if (ipp) {
                        for_each(idp, ipp) {
                           lListElem *ip;

                           jep = lAddElemUlong(&jl, JB_job_number, 
                                    atol(lGetString(idp, ID_str)), dp);
                           lSetList(jep, JB_ja_structure, 
                                 lCopyList("", lGetList(idp, ID_ja_structure)));
                           get_ja_task_ids(jep, &start, &end, &step); 
                           for_each(ip, lGetList(idp, ID_ja_structure)) {
                              start = lGetUlong(ip, RN_min);
                              end = lGetUlong(ip, RN_max);
                              step = lGetUlong(ip, RN_step);
                              for (; start<=end; start+=step) {
                                 lAddElemUlong(&jat_list, JAT_task_number, 
                                                   start, JAT_Type);
                              }
                           }
                        }
                        lSetList(jep, JB_ja_tasks, jat_list);
                        ipp = lFreeList(ipp);
                     }
                  }
                  break;
               case STR:
                  jep = lAddElemStr(&jl, STR, str, dp);
                  lSetList(jep, JB_ja_tasks, NULL);
                  break;
            }
         }
      }
   }

   DEXIT;
   return jl;
}

/*-------------------------------------------------------------------------*/
static void qmonJobPriority(
Widget w,
XtPointer cld,
XtPointer cad 
) {
   lList *jl = NULL;
   lList *rl = NULL;
   lListElem *jep = NULL;
   lList *alp = NULL;
   Boolean status_ask = False;
   int new_priority = 0;

   lDescr prio_descr[] = {
      {JB_job_number, lUlongT},
      {JB_ja_tasks, lListT},
      {JB_ja_structure, lListT},
      /* optional fields */
      {JB_priority, lUlongT},
      {NoName, lEndT}
   };
   
   DENTER(GUI_LAYER, "qmonJobPriority");
   
   rl = qmonJobBuildSelectedList(job_running_jobs, prio_descr, JB_job_number);
   
   /*
   ** set priority works only for pending jobs in SGE mode
   */
   jl = qmonJobBuildSelectedList(job_pending_jobs, prio_descr, JB_job_number);

   if (!jl && rl)
      jl = rl;
   else if (rl)
      lAddList(jl, rl);
   
   
   if (jl)
      status_ask = XmtAskForInteger(w, NULL, 
                        "@{Enter a new priority for the selected jobs}", 
                        &new_priority, -1023, 1024, NULL); 
   else
      qmonMessageShow(w, True, "@{There are no jobs selected !}");

   if (jl && status_ask) {
      for_each(jep, jl)
         lSetUlong(jep, JB_priority, BASE_PRIORITY + new_priority);
      /*
      ** call here gdi_qmod_job instead of qmonDelList
      */
      alp = sge_gdi(SGE_JOB_LIST, SGE_GDI_MOD, &jl, NULL, NULL); 
   
      qmonMessageBox(w, alp, 0);

      jl = lFreeList(jl);
      alp = lFreeList(alp);

      /*
      ** we use the callback to get the new job list from master
      ** cause the sge_gdi doesn't change the local list
      ** so updateJobList() is not sufficient
      */
      updateJobListCB(w, NULL, NULL);
      XbaeMatrixDeselectAll(job_pending_jobs);
      XbaeMatrixDeselectAll(job_running_jobs);

   }
   
   DEXIT;
}

/*-------------------------------------------------------------------------*/
static void qmonJobHold(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
   lList *jl = NULL;
   lList *rl = NULL;
   lListElem *jep = NULL;
   lList *alp = NULL;
   Boolean status_ask = False;
   Cardinal new_hold = 0;
   lListElem *job = NULL;
   lListElem *jatep = NULL;

   lDescr hold_descr[] = {
      {JB_job_number, lUlongT},
      {JB_ja_tasks, lListT},
      {JB_ja_structure, lListT},
      {NoName, lEndT}
   };
   
   DENTER(GUI_LAYER, "qmonJobHold");
   
   rl = qmonJobBuildSelectedList(job_running_jobs, hold_descr, JB_job_number);
   
   /*
   ** set priority works only for pending jobs
   */
   jl = qmonJobBuildSelectedList(job_pending_jobs, hold_descr, JB_job_number);
   
   if (!jl && rl)
      jl = rl;
   else if (rl)
      lAddList(jl, rl);
   
   if (jl) {
      StringBufferT dyn_tasks = {NULL, 0};
      StringBufferT dyn_oldtasks = {NULL, 0};

      /*
      ** get the first job in list and its first task to preset 
      ** the hold dialog
      */
      job = lGetElemUlong(qmonMirrorList(SGE_JOB_LIST), JB_job_number, 
                              lGetUlong(lFirst(jl), JB_job_number));
      new_hold = lGetUlong(lFirst(lGetList(job, JB_ja_tasks)), JAT_hold);

      if (lGetNumberOfElem(jl) == 1 && is_array(job)) {
         get_taskrange_str(lGetList(job, JB_ja_tasks), &dyn_tasks);
         sge_string_append(&dyn_oldtasks, dyn_tasks.s);
      }

      status_ask = AskForHold(w, &new_hold, &dyn_tasks); 

      if (jl && status_ask) {
         for_each(jep, jl) {
            if (dyn_tasks.s && strcmp(dyn_tasks.s, "") && 
                !strcmp(dyn_tasks.s, dyn_oldtasks.s)) {
               for_each (jatep, lGetList(jep, JB_ja_tasks)) {
                  lSetUlong(jatep, JAT_hold, new_hold | MINUS_H_CMD_SET);
                  DPRINTF(("Hold for" u32 "." u32 " is " u32 "\n", 
                              lGetUlong(jep, JB_job_number), 
                              lGetUlong(jatep, JAT_task_number),
                              new_hold | MINUS_H_CMD_SET));
               }
            }
            else {
               lList *range_list = NULL;
/*                lList *jat_list = NULL; */
               lListElem *range = NULL;
               u_long32 start=1, end=1, step=1;
               /* reset all tasks */
               for_each (jatep, lGetList(jep, JB_ja_tasks)) {
                  lSetUlong(jatep, JAT_hold, 0);
               }
 
               range_list = parse_ranges(dyn_tasks.s, 0, 1, NULL, NULL, 
                                          INF_NOT_ALLOWED);
 
               if (!range_list) {
                  lListElem *tap = NULL;
                  tap = lAddElemUlong(&range_list, RN_min, 1, RN_Type);
                  lSetUlong(tap, RN_max, 1);
                  lSetUlong(tap, RN_step, 1);
               }
               for_each (range, range_list) {
                  start = lGetUlong(range, RN_min);
                  end = lGetUlong(range, RN_max);
                  step = lGetUlong(range, RN_step);
                  for (; start<=end; start+=step) {
                     lListElem *jatep;
                     jatep = lGetElemUlong(lGetList(jep, JB_ja_tasks),
                                           JAT_task_number, start);
                     if (jatep) {
                        lSetUlong(jatep, JAT_hold, new_hold | MINUS_H_CMD_SET);
                        DPRINTF(("Hold for" u32 "." u32 " is " u32 "\n",
                                 lGetUlong(jep, JB_job_number),
                                 lGetUlong(jatep, JAT_task_number),      
                                 new_hold | MINUS_H_CMD_SET));
                     }
                  }
               }
               range_list = lFreeList(range_list); 
            }
         }
         /*
         ** call here gdi_qmod_job 
         */
         alp = sge_gdi(SGE_JOB_LIST, SGE_GDI_MOD, &jl, NULL, NULL); 
      
         qmonMessageBox(w, alp, 0);

         jl = lFreeList(jl);
         alp = lFreeList(alp);

         /*
         ** we use the callback to get the new job list from master
         ** cause the sge_gdi doesn't change the local list
         ** so updateJobList() is not sufficient
         */
         updateJobListCB(w, NULL, NULL);
         XbaeMatrixDeselectAll(job_pending_jobs);
         XbaeMatrixDeselectAll(job_running_jobs);

      }
      sge_string_free(&dyn_tasks);
      sge_string_free(&dyn_oldtasks);
   }
   else
      qmonMessageShow(w, True, "@{There are no jobs selected !}");


   DEXIT;
}

/*-------------------------------------------------------------------------*/
static void qmonJobQalter(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
#ifdef QALTER_RUNNING
   lList *rl = NULL;
#endif
   lList *pl = NULL;
   tSubmitMode data;

   DENTER(GUI_LAYER, "qmonJobQalter");
#ifdef QALTER_RUNNING
   rl = qmonJobBuildSelectedList(job_running_jobs, JB_Type, JB_job_number);
#endif
   /*
   ** qalter works for pending  & running jobs
   */
   pl = qmonJobBuildSelectedList(job_pending_jobs, JB_Type, JB_job_number);

#ifdef QALTER_RUNNING
   if (!pl && rl)
      pl = rl;
   else if (rl)
      lAddList(pl, rl);
#endif
   /*
   ** call the submit dialog for pending jobs
   */
   if (pl && lGetNumberOfElem(pl) == 1) {
      data.mode = SUBMIT_QALTER_PENDING;
      data.job_id = lGetUlong(lFirst(pl), JB_job_number);
      qmonSubmitPopup(w, (XtPointer)&data, NULL);
      lFreeList(pl);
      XbaeMatrixDeselectAll(job_pending_jobs);
#ifdef QALTER_RUNNING
      XbaeMatrixDeselectAll(job_running_jobs);
#endif
   }
   else
#ifndef QALTER_RUNNING
      qmonMessageShow(w, True, "@{Select one pending job for Qalter !}");
#else
      qmonMessageShow(w, True, "@{Select one job for Qalter !}");
#endif
   
   DEXIT;
}



/*-------------------------------------------------------------------------*/
static void qmonJobHandleEnterLeave(
Widget w,
XtPointer cld,
XEvent *ev,
Boolean *ctd 
) {
   /*
   int root_x, root_y, pos_x, pos_y;
   Window root, child;
   unsigned int keys_buttons;
   */
   char line[BUFSIZ];
   static int prev_row = -1;
   int row, col;
   String str;
   int job_nr;
   lListElem *jep;
   String job_info;
   
   DENTER(GUI_LAYER, "qmonJobHandleEnterLeave");
   
   switch (ev->type) {
      case MotionNotify:
         if (qmonBrowserObjectEnabled(BROWSE_JOB)) {
            /* 
            ** XQueryPointer(XtDisplay(w), XtWindow(w), 
            **            &root, &child, &root_x, &root_y,
            **            &pos_x, &pos_y, &keys_buttons);
            */
            if (XbaeMatrixGetEventRowColumn(w, ev, &row, &col)) {
               if ( row != prev_row ) {
                  prev_row = row;
                  /* locate the job */
                  str = XbaeMatrixGetCell(w, row, 0);
                  if (str && *str != '\0') {
                     job_nr = atoi(str);
                     jep = lGetElemUlong(qmonMirrorList(SGE_JOB_LIST), 
                                          JB_job_number, (u_long32)job_nr);
                     if (jep) {
                        sprintf(line, "+++++++++++++++++++++++++++++++++++++++++++\n");  
                        qmonBrowserShow(line);
                        job_info = qmonJobShowBrowserInfo(jep);      
                        qmonBrowserShow(job_info);
                        qmonBrowserShow(line);
                     }
                  }
               }
            }
         }
         break;
   }
   DEXIT;
}

/*-------------------------------------------------------------------------*/
static String qmonJobShowBrowserInfo(
lListElem *jep 
) {
#define WIDTH  "%s%-30.30s"

   static char info[60000];
   char buf[1024];
   char *str;

/*    int status; */

   DENTER(GUI_LAYER, "qmonJobShowBrowserInfo");

   sprintf(info, WIDTH"%d\n", "\n","Job:", (int)lGetUlong(jep, JB_job_number));
   sprintf(info, WIDTH"%s\n", info, "Job Name:", 
                  lGetString(jep, JB_job_name));
   sprintf(info, WIDTH"%s\n", info, "Job Script:", 
                  lGetString(jep, JB_script_file));
   sprintf(info, WIDTH"%s\n", info, "Owner:", 
                  lGetString(jep, JB_owner));
   sprintf(info, WIDTH"%d\n", info, "Priority:", 
                  (int)(lGetUlong(jep, JB_priority) - BASE_PRIORITY));

#ifdef FIXME
   if (lGetList(jep, JB_jid_predecessor_list) || lGetUlong(jep, JB_hold)) {
      sprintf(info, WIDTH"%s\n", info, "Status:", 
                     "Hold");
   }
   else {
      status = lGetUlong(jep, JB_status);
      switch (status) {
         case JRUNNING:
            sprintf(info, WIDTH"%s\n", info, "Status:", 
                     "Running");
            break;
         case JIDLE:
            sprintf(info, WIDTH"%s\n", info, "Status:", 
                     "Idle");
            break;
         case JTRANSITING:
            sprintf(info, WIDTH"%s\n", info, "Status:", 
                     "Transiting");
            break;
      }
   }


   sprintf(info, WIDTH"%s\n", info, "Submission Time:", 
               sge_ctime(lGetUlong(jep, JB_submission_time)));

   sprintf(info, WIDTH"%s\n", info, "Start Time:", 
      lGetUlong(jep, JB_start_time) ? sge_ctime(lGetUlong(jep, JB_start_time)):
      "-none-");
#endif

#if 0
   sprintf(info, WIDTH"%s\n", info, "Execution Time:", 
               sge_ctime(lGetUlong(jep, JB_execution_time)));
   str = lGetString(jep, JB_sge_o_home);
   sprintf(info, WIDTH"%s\n", info, "SGE_O_HOME:", str ? str : "");

   str = lGetString(jep, JB_sge_o_log_name);
   sprintf(info, WIDTH"%s\n", info, "SGE_O_LOG_NAME:", str ? str : ""); 

   str = lGetString(jep, JB_sge_o_path);
   sprintf(info, WIDTH"%s\n", info, "SGE_O_PATH:", str ? str : ""); 

   str = lGetString(jep, JB_sge_o_mail);
   sprintf(info, WIDTH"%s\n", info, "SGE_O_MAIL:", str ? str : ""); 

   str = lGetString(jep, JB_sge_o_shell);
   sprintf(info, WIDTH"%s\n", info, "SGE_O_SHELL:", str ? str : ""); 

   str = lGetString(jep, JB_sge_o_tz);
   sprintf(info, WIDTH"%s\n", info, "SGE_O_TZ:", str ? str : ""); 

   str = lGetString(jep, JB_sge_o_workdir);
   sprintf(info, WIDTH"%s\n", info, "SGE_O_WORKDIR:", str ? str : ""); 

   str = lGetString(jep, JB_sge_o_host);
   sprintf(info, WIDTH"%s\n", info, "SGE_O_HOST:", str ? str : ""); 
#endif

   str = lGetString(jep, JB_cell);
   sprintf(info, WIDTH"%s\n", info, "Cell:", str ? str : "default"); 

#if 0
   str = lGetString(jep, JB_directive_prefix);
   sprintf(info, WIDTH"%s\n", info, "Directive Prefix:", str ? str : "#$"); 
#endif

   sprintf(info, WIDTH"%s\n", info, "Checkpoint Object:", 
         lGetString(jep, JB_checkpoint_object) ? 
            lGetString(jep, JB_checkpoint_object) : "");

   if (lGetString(jep, JB_pe)) {
      show_ranges(buf, 0, NULL, lGetList(jep, JB_pe_range));
      sprintf(info, WIDTH"%s %s\n", info, "Requested PE:", 
               lGetString(jep, JB_pe), buf);
   }

#ifdef FIXME
   if (lGetString(jep, JB_granted_pe)) {
      lListElem *gdil_ep;
      u_long32 pe_slots = 0;
      for_each (gdil_ep, lGetList(jep, JB_granted_destin_identifier_list))
         pe_slots += lGetUlong(gdil_ep, JG_slots);
      sprintf(info, WIDTH"%s " u32 "\n", info, "Granted PE:", 
               lGetString(jep, JB_pe), pe_slots);
   }
#endif

   strcpy(buf, "");
   unparse_resources(NULL, buf, sizeof(buf), 
                     lGetList(jep, JB_hard_resource_list));
   sprintf(info, WIDTH"%s", info, "Hard Resources:", buf);
   if (*buf=='\0')
      strcat(info, "\n");
               
   strcpy(buf, "");
   unparse_resources(NULL, buf, sizeof(buf), 
                     lGetList(jep, JB_soft_resource_list));
   sprintf(info, WIDTH"%s", info, "Soft Resources:", buf);
   if (*buf=='\0')
      strcat(info, "\n");


   
/*    lWriteListTo(lGetList(jep, JB_qs_args), stdout); */
   
   DPRINTF(("info is %d long\n", strlen(info)));
   
   DEXIT;
   return info;
}

/*-------------------------------------------------------------------------*/
static Boolean AskForHold(
Widget w,
Cardinal *hold,
StringBufferT *dyn_tasks 
) {
   static tAskHoldInfo AskHoldInfo;      
   Widget shell = XmtGetShell(w);
   Widget hold_layout, hold_cancel, hold_ok;
   static Widget hold_flags, hold_tasks;

   DENTER(GUI_LAYER, "AskForHold");

   /* 
   ** make sure the shell we pop up over is not a menu shell! 
   */
   while(XtIsOverrideShell(shell)) 
      shell = XmtGetShell(XtParent(shell));
 
   /*
   ** create the hold popup dialog
   */
   if (!AskHoldInfo.AskHoldDialog) {
      AskHoldInfo.AskHoldDialog = XmtBuildQueryDialog( shell, "qmon_hold",
                           NULL, 0,
                           "hold_cancel", &hold_cancel,
                           "hold_ok", &hold_ok,
                           "hold_flags", &hold_flags,
                           "hold_tasks", &hold_tasks,
                           NULL);
      AskHoldInfo.AskHoldFlags = hold_flags;
      AskHoldInfo.AskHoldTasks = hold_tasks;
      XtVaSetValues( AskHoldInfo.AskHoldDialog,
                     XmNdefaultButton, hold_ok,
                     NULL);
      XtAddCallback(hold_ok, XmNactivateCallback, 
                  AskHoldOkCB, (XtPointer)&AskHoldInfo);
      XtAddCallback(hold_cancel, XmNactivateCallback, 
                  AskHoldCancelCB, (XtPointer)&AskHoldInfo);
      
      XmtAddDeleteCallback(XtParent(AskHoldInfo.AskHoldDialog), XmDO_NOTHING,
                      AskHoldCancelCB, (XtPointer)&AskHoldInfo);

   }
   hold_layout = AskHoldInfo.AskHoldDialog;
   hold_flags = AskHoldInfo.AskHoldFlags;
   hold_tasks = AskHoldInfo.AskHoldTasks;
   
   /* 
   ** Tell the dialog who it is transient for 
   */
   XtVaSetValues( XtParent(hold_layout), 
                  XtNtransientFor, shell, 
                  NULL);
   /*
   ** preset with default values
   */
   XmtChooserSetState(hold_flags, *hold, False);
   if (dyn_tasks->s && (dyn_tasks->s)[0] != '\0') {
      XtSetSensitive(hold_tasks, True);
      XmtInputFieldSetString(hold_tasks, dyn_tasks->s);
   }
   else {
      XtSetSensitive(hold_tasks, False);
      XmtInputFieldSetString(hold_tasks, "");
   }   

   /* 
   ** position, set initial focus, and pop up the dialog 
   */
   XmtDialogPosition(XtParent(hold_layout), shell);
   XtManageChild(hold_layout);

   /*
   ** Enter a recursive event loop.
   ** The callback registered on the okay and cancel buttons when
   ** this dialog was created will cause info->button to change
   ** when one of those buttons is pressed.
   */
   AskHoldInfo.blocked = True;
   XmtBlock(shell, &AskHoldInfo.blocked);

   /* pop down the dialog */
   XtUnmanageChild(hold_layout);

   /* make sure what is underneath gets cleaned up
   ** (the calling routine might act on the user's returned
   ** input and not handle events for awhile.)
   */
   XSync(XtDisplay(hold_layout), 0);
   XmUpdateDisplay(hold_layout);

   /*
   ** if the user clicked Ok, return the hold state
   */
   if (AskHoldInfo.button == XmtOkButton) {
      String ts;
      *hold = XmtChooserGetState(hold_flags);
      ts = XmtInputFieldGetString(hold_tasks);
      sge_string_free(dyn_tasks);
      sge_string_append(dyn_tasks, ts ? ts : ""); 
      DEXIT;
      return True;
   }
   else {
      DEXIT;
      return False;
   }
}

   
static void AskHoldOkCB(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
    tAskHoldInfo *info = (tAskHoldInfo *)cld;
    info->blocked = False;
    info->button = XmtOkButton;
}

static void AskHoldCancelCB(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
    tAskHoldInfo *info = (tAskHoldInfo *)cld;
    info->blocked = False;
    info->button = XmtCancelButton;
}

/*-------------------------------------------------------------------------*/
static void qmonJobScheddInfo(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
   StringBufferT sb = {NULL, 0};
   lList *jl = NULL;

   lDescr info_descr[] = {
      {STR, lStringT},
      {NoName, lEndT}
   };
   
   DENTER(GUI_LAYER, "qmonJobScheddInfo");
   
   jl = qmonJobBuildSelectedList(job_pending_jobs, info_descr, STR);
   
   qmonBrowserOpen(w, NULL, NULL);
   if (jl ? (show_info_for_jobs(jl, NULL, NULL, &sb) == 0) :  
            (show_info_for_job(NULL, NULL, &sb) == 0) && sb.s) {
      qmonBrowserShow(sb.s);
      free(sb.s);
   }
   else 
      qmonBrowserShow("---------Could not get scheduling info--------\n");

   DEXIT;
}

/*-------------------------------------------------------------------------*/
static void qmonJobFolderChange(w, cld, cad)
Widget w;
XtPointer cld, cad;
{
   XmTabCallbackStruct *cbs = (XmTabCallbackStruct *) cad;

   DENTER(GUI_LAYER, "qmonJobFolderChange");
   
   DPRINTF(("%s\n", XtName(cbs->tab_child)));

   if (!strcmp(XtName(cbs->tab_child), "job_pending")) {
      XtSetSensitive(job_schedd_info, True);
   }
   else {
      XtSetSensitive(job_schedd_info, False);
   }
   if (!strcmp(XtName(cbs->tab_child), "job_running")) {
      XtSetSensitive(job_reschedule, True);
   }
   else {
      XtSetSensitive(job_reschedule, False);
   }
   if (!strcmp(XtName(cbs->tab_child), "job_zombie")) {
      XtSetSensitive(force_toggle, False);
      XtSetSensitive(job_suspend, False);
      XtSetSensitive(job_unsuspend, False);
      XtSetSensitive(job_delete, False);
      XtSetSensitive(job_hold, False);
      XtSetSensitive(job_qalter, False);
      XtSetSensitive(job_error, False);
      XtSetSensitive(job_priority, False);
   }
   else {
      XtSetSensitive(force_toggle, True);
      XtSetSensitive(job_suspend, True);
      XtSetSensitive(job_unsuspend, True);
      XtSetSensitive(job_delete, True);
      XtSetSensitive(job_hold, True);
      XtSetSensitive(job_qalter, True);
      XtSetSensitive(job_error, True);
      XtSetSensitive(job_priority, True);
   }

   DEXIT;
}

