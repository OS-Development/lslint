#include "lslmini.hh"

void LLASTNode::final_walk() {
   LLASTNode *node;
   final_pre_checks();
   for ( node = get_children(); node; node = node->get_next() )
      node->final_walk();
   final_post_checks();
}

bool allret(LLASTNode *p) {
   bool ret = false;
   if (p->get_node_type() == NODE_STATEMENT && p->get_node_sub_type() == NODE_RETURN_STATEMENT) {
      // TODO check next value here for unreachable code
      return true;
   }
   else if (p->get_node_type() == NODE_STATEMENT && p->get_node_sub_type() == NODE_IF_STATEMENT) {
      bool true_branch = p->get_child(1) && allret(p->get_child(1));
      bool false_branch = p->get_child(2) && allret(p->get_child(2));

      return (true_branch && false_branch);
   }
   else if (p->get_node_type() == NODE_STATEMENT && p->get_node_sub_type() == NODE_COMPOUND_STATEMENT) {
      LLASTNode *q;
      for (q = p->get_children(); q; q = q->get_next()) {
         ret |= allret(q);
      }
   }
   else {
#if 0
      if (p->get_next()) {
         ret |= allret(p->get_next());
      }
      if (p->get_children()) {
         ret |= allret(p->get_children());
      }
#endif
   }
   return ret;
}

void LLScriptGlobalFunction::final_pre_checks() {
   LLScriptIdentifier *id = (LLScriptIdentifier *) get_child(0);
   //LLScriptFunctionDec *decl = (LLScriptFunctionDec *) get_child(1);
   LLScriptStatement *statement = (LLScriptStatement *) get_child(2);

   if (id->get_symbol() == NULL) {
      id->resolve_symbol(SYM_FUNCTION);
   }

   if (id->get_symbol() != NULL) {
      LLScriptType *type = id->get_symbol()->get_type();

      if (type->get_itype() != LST_NULL && !allret(statement)) {
         ERROR(IN(get_child(0)), E_NOT_ALL_PATHS_RETURN);
      }
   }
}

static bool isKey(const char *value) {
   if (strlen(value) != 36 || value[8] != '-' || value[13] != '-' || value[18] != '-' || value[23] != '-')
      return false;

   char buf[37];
   strcpy(buf, value);
   // change dashes to valid letters, and check the whole string
   buf[8] = 'A'; buf[13] = 'A'; buf[18] = 'A'; buf[23] = 'A';
   return strspn(buf, "0123456789ABCDEFabcdef") == 36;
}

void check_cond(LLScriptExpression *expr, bool warn_if_true) {
   // see if expression is constant
   if ( expr->get_constant_value() != NULL ) {
      int truth = 2; // 2 denotes that it hasn't been handled
      LLNodeSubType type = expr->get_constant_value()->get_node_sub_type();
      if ( type == NODE_INTEGER_CONSTANT ) {
         truth = ((LLScriptIntegerConstant*)expr->get_constant_value())->get_value() != 0;
      } else if ( type == NODE_FLOAT_CONSTANT ) {
         truth = ((LLScriptFloatConstant*)expr->get_constant_value())->get_value() != 0.f;
      } else if ( type == NODE_STRING_CONSTANT ) {
         truth = ((LLScriptStringConstant*)expr->get_constant_value())->get_value()[0] != 0;
      } else if ( type == NODE_KEY_CONSTANT) {
         const char *value = ((LLScriptKeyConstant*)expr->get_constant_value())->get_value();
         truth = value && isKey(value) && strcmp(value, "00000000-0000-0000-0000-000000000000");
      } else if ( type == NODE_VECTOR_CONSTANT ) {
         LLVector *value = ((LLScriptVectorConstant*)expr->get_constant_value())->get_value();
         truth = value->x != 0.f || value->y != 0.f || value->z != 0.f;
      } else if ( type == NODE_QUATERNION_CONSTANT ) {
         LLQuaternion *value = ((LLScriptQuaternionConstant*)expr->get_constant_value())->get_value();
         truth = value->x != 0.f || value->y != 0.f || value->z != 0.f || value->s != 1.0f;
      } else if ( type == NODE_LIST_CONSTANT ) {
         int length = ((LLScriptListConstant*)expr->get_constant_value())->get_length();
         truth = length > (mono_mode ? 0 : 1);
      } else {
         // you can't handle the truth
      }

      if (truth != 2) {
         // valid
         if (truth) {
            if (warn_if_true) {
               ERROR( IN(expr), W_CONDITION_ALWAYS_TRUE );
            }
         } else {
            ERROR( IN(expr), W_CONDITION_ALWAYS_FALSE );
         }
      }
   }

   // see if expression is an assignment
   if ( expr->get_operation() == '=' ) {
      ERROR( IN(expr), W_ASSIGNMENT_IN_COMPARISON );
   }
}

void LLScriptListExpression::final_pre_checks() {
   LLASTNode *elem;
   for ( elem = get_children(); elem; elem = elem->get_next() )
      if ( elem->get_type()->get_itype() == LST_LIST )
          ERROR( IN(elem), E_LIST_IN_LIST );
}

void LLScriptListConstant::final_pre_checks() {
   LLASTNode *elem;
   for ( elem = get_children(); elem; elem = elem->get_next() ) {
      if ( elem->get_type()->get_itype() == LST_LIST ) {
         if (elem->get_node_type() == NODE_SIMPLE_ASSIGNABLE) {
            ERROR( IN(elem->get_child(0)), E_LIST_IN_LIST );
         } else {
            ERROR( IN(elem), E_LIST_IN_LIST );
         }
      }
   }
}

void LLScriptTypecastExpression::final_pre_checks() {
   if (!mono_mode && get_type()->get_itype() == LST_LIST) {
      // Check if it only has a variable of type string or key
      LLASTNode *child = get_children();
      if (child && !child->get_next() && child->get_node_type() == NODE_EXPRESSION && child->get_node_sub_type() == NODE_LVALUE_EXPRESSION) {
         // We're on the right track. Go one level deeper.
         child = child->get_children();
         if (child && !child->get_next() && child->get_node_type() == NODE_IDENTIFIER) {
            LST_TYPE type = child->get_type()->get_itype();
            // A-ha!
            if (type == LST_STRING) {
                ERROR(HERE, W_KEY_OR_STR_TO_LIST, "string", "key", "string");
            }  else if (type == LST_KEY) {
                ERROR(HERE, W_KEY_OR_STR_TO_LIST, "key", "string", "key");
            }
         }
      }

   }
}

void LLScriptIfStatement::final_pre_checks() {
   check_cond((LLScriptExpression*)get_child(0), true);
}

void LLScriptWhileStatement::final_pre_checks() {
   check_cond((LLScriptExpression*)get_child(0), false);
}

void LLScriptDoStatement::final_pre_checks() {
   check_cond((LLScriptExpression*)get_child(1), false);
}

void LLScriptForStatement::final_pre_checks() {
   check_cond((LLScriptExpression*)get_child(1), false);
}

void LLScriptEventHandler::final_pre_checks() {
   LLASTNode *node = NULL;
   bool is_last = true;
   int found = 0;
   LLScriptIdentifier *id = (LLScriptIdentifier *)get_child(0);

   // check for duplicates
   for (node = get_parent()->get_children(); node; node = node->get_next()) {
      if ( node->get_node_type() != NODE_EVENT_HANDLER )
         continue;
      LLScriptIdentifier *other_id = (LLScriptIdentifier *)node->get_child(0);
      if (!strcmp(id->get_name(), other_id->get_name())) {
         found++;
         is_last = (node == this);
      }
   }
   if (found > 1 && is_last) {
      ERROR( HERE, E_MULTIPLE_EVENT_HANDLERS, id->get_name() );
   }

   // check parameters
   if (id->get_symbol() == NULL) {
      id->resolve_symbol(SYM_EVENT);
   }

   if (id->get_symbol() != NULL) {
      // check argument types
      LLScriptFunctionDec       *function_decl;
      LLScriptIdentifier        *declared_param_id;
      LLScriptIdentifier        *passed_param_id;
      int                        param_num = 1;

      function_decl         = id->get_symbol()->get_function_decl();
      declared_param_id     = (LLScriptIdentifier*) function_decl->get_children();
      passed_param_id       = (LLScriptIdentifier*) get_child(1)->get_children();

      while ( declared_param_id != NULL && passed_param_id != NULL ) {
         if ( !passed_param_id->get_type()->can_coerce(
                  declared_param_id->get_type()) ) {
            ERROR( IN(passed_param_id), E_ARGUMENT_WRONG_TYPE_EVENT,
                  passed_param_id->get_type()->get_node_name(),
                  param_num,
                  id->get_name(),
                  declared_param_id->get_type()->get_node_name(),
                  declared_param_id->get_name()
                 );
            return;
         }
         passed_param_id   = (LLScriptIdentifier*) passed_param_id->get_next();
         declared_param_id = (LLScriptIdentifier*) declared_param_id->get_next();
         ++param_num;
      }

      if ( passed_param_id != NULL ) {
         // printf("too many, extra is %s\n", passed_param_id->get_name());
         ERROR( IN(passed_param_id), E_TOO_MANY_ARGUMENTS_EVENT, id->get_name() );
      } else if ( declared_param_id != NULL ) {
         // printf("too few, extra is %s\n", declared_param_id->get_name());
         ERROR( HERE, E_TOO_FEW_ARGUMENTS_EVENT, id->get_name() );
      }
   }
   else {
      ERROR( HERE, E_INVALID_EVENT, id->get_name());
   }

}

void LLScriptSwitchStatement::final_pre_checks() {
   int num_defaults = 0;
   LLASTNode *node = get_children(), *node2, *second_default = NULL;

   // first child is switch expression
   if (node->get_constant_value() != NULL) {
      // rather than warning on each case, warn once at the top
      ERROR( IN(node), W_CONSTANT_SWITCH );
   }
   LLScriptType *switch_type = node->get_type();
   if (switch_type->get_itype() == LST_LIST) {
      // This error may sound a bit awkward to the user, but since == is
      // used internally by the switch converter, it should be OK.
      ERROR( IN(node), W_LIST_COMPARE );
   }

   // subsequent children are case or default blocks
   for (node = node->get_next() ; node; node = node->get_next() ) {
      // node is a case block or a default block (default blocks have NULL
      // as the first child)
      if (node->get_child(0)->get_node_type() != NODE_NULL) {
         LLScriptConstant *const1 = node->get_child(0)->get_constant_value();
         if (const1) {
            // check type compatibility
            if (switch_type->get_result_type(EQ, const1->get_type())) {

               // compare every case block with every other case block
               // (skipping default blocks)
               for ( node2 = node->get_next(); node2; node2 = node2->get_next() ) {
                  if (node2->get_child(0)) {
                     LLScriptConstant *const2 = node2->get_child(0)->get_constant_value();
                     if (const2) {
                        LLScriptIntegerConstant *comparison = (LLScriptIntegerConstant *)const1->operation(EQ, const2, const1->get_lloc());
                        if (comparison) {
                           if (comparison->get_value()) {
                              ERROR( IN(const2), W_DUPLICATE_CASE );
                           } // else it will err later, as type equality is transitive
                           delete comparison;
                        }
                     }
                  }
               }

            } else {
               ERROR( IN(const1), E_INCOMPATIBLE_CASE_TYPE );
            }

         }
      } else {
         if (++num_defaults == 2) {
            second_default = node;
         }
      }
   }
   if (! num_defaults) {
      ERROR( HERE, W_SWITCH_NO_DEFAULT );
   } else if (num_defaults > 1) {
      ERROR( IN(second_default), E_SWITCH_MULTIPLE_DEFAULTS );
   }

   script->inc_switchlevel();
}

void LLScriptSwitchStatement::final_post_checks() {
   script->dec_switchlevel();
}

void LLScriptBreakStatement::final_pre_checks() {
   if (!script->get_switchlevel()) {
      ERROR( HERE, E_BREAK_WITHOUT_SWITCH );
   }
}
