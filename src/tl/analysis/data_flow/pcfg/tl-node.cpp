/*--------------------------------------------------------------------
  (C) Copyright 2006-2012 Barcelona Supercomputing Center
                          Centro Nacional de Supercomputacion
  
  This file is part of Mercurium C/C++ source-to-source compiler.
  
  See AUTHORS file in the top level directory for information 
  regarding developers and contributors.
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.
  
  Mercurium C/C++ source-to-source compiler is distributed in the hope
  that it will be useful, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the GNU Lesser General Public License for more
  details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with Mercurium C/C++ source-to-source compiler; if
  not, write to the Free Software Foundation, Inc., 675 Mass Ave,
  Cambridge, MA 02139, USA.
--------------------------------------------------------------------*/

#include "cxx-codegen.h"
#include "cxx-process.h"
#include "tl-constants-analysis.hpp"
#include "tl-edge.hpp"
#include "tl-extended-symbol-utils.hpp"
#include "tl-iv-analysis.hpp"
#include "tl-node.hpp"

namespace TL {
namespace Analysis {
    
    Node::Node()
        : _id(-1), _entry_edges(), _exit_edges(), _visited(false), _visited_aux(false), _has_deps_computed(false)
    {
        set_data(_NODE_TYPE, UNCLASSIFIED_NODE);
    }
    
    Node::Node(int& id, Node_type ntype, Node* outer_node)
        : _id(++id), _entry_edges(), _exit_edges(), _visited(false), _visited_aux(false), _has_deps_computed(false)
    {
        set_data(_NODE_TYPE, ntype);
        
        if (outer_node != NULL)
        {
            set_data(_OUTER_NODE, outer_node);
        }

        if (ntype == GRAPH_NODE)
        {
            set_data(_ENTRY_NODE, new Node(id, BASIC_ENTRY_NODE, NULL));
            int a = -2; set_data(_EXIT_NODE, new Node(a, BASIC_EXIT_NODE, NULL));
        }
    }
    
    Node::Node(int& id, Node_type type, Node* outer_node, ObjectList<Nodecl::NodeclBase> nodecls)
        : _id(++id), _entry_edges(), _exit_edges(), _visited(false), _visited_aux(false), _has_deps_computed(false)
    {        
        set_data(_NODE_TYPE, type);
        
        if (outer_node != NULL)
        {    
            set_data(_OUTER_NODE, outer_node);
        }
        
        set_data(_NODE_STMTS, nodecls);
    }
    
    Node::Node(int& id, Node_type type, Node* outer_node, Nodecl::NodeclBase nodecl)
        : _id(++id), _entry_edges(), _exit_edges(), _visited(false), _visited_aux(false), _has_deps_computed(false)
    {      
        set_data(_NODE_TYPE, type);
        
        if (outer_node != NULL)
        {    
            set_data(_OUTER_NODE, outer_node);
        }
        
        set_data(_NODE_STMTS, ObjectList<Nodecl::NodeclBase>(1,nodecl));
    }
    
    bool Node::operator==(const Node& node) const
    {
        return (_id == node._id);
    }
    
    void Node::erase_entry_edge(Node* source)
    {
        ObjectList<Edge*>::iterator it;
        for (it = _entry_edges.begin(); 
                it != _entry_edges.end();
                ++it)
        {
            if ((*it)->get_source() == source)
            {
                _entry_edges.erase(it);
                --it;   // Decrement to allow the correctness of the comparison outside the loop
                break;
            }
        }
        if (it == _entry_edges.end())
        {
            std::cerr << " ** Node.cpp :: erase_entry_edge() ** "
                    << "Trying to delete an non-existent edge " 
                    << "between nodes '" << source->_id << "' and '" << _id << "'" << std::endl;
        }
    }
    
    void Node::erase_exit_edge(Node* target)
    {
        ObjectList<Edge*>::iterator it;
        for (it = _exit_edges.begin(); 
                it != _exit_edges.end();
                ++it)
        {
            if ((*it)->get_target() == target)
            {
                _exit_edges.erase(it);
                --it;   // Decrement to allow the correctness of the comparison outside the loop
                break;
            }
        }
        if (it == _exit_edges.end())
        {
            std::cerr << " ** Node.cpp :: exit_entry_edge() ** "
                    << "Trying to delete an non-existent edge " 
                    << "between nodes '" << _id << "' and '" << target->_id << "'" << std::endl;  
    
        }
    }
    
    int Node::get_id() const
    {
        return _id;
    }
    
    void Node::set_id(int id)
    {
        _id = id;
    }
    
    bool Node::is_visited() const
    {
        return _visited;
    }

    bool Node::is_visited_aux() const
    {
        return _visited_aux;
    }

    void Node::set_visited(bool visited)
    {
        _visited = visited;
    }

    void Node::set_visited_aux(bool visited)
    {
        _visited_aux = visited;
    }
    
    bool Node::has_deps_computed()
    {
        return _has_deps_computed;
    }
    
    void Node::set_deps_computed()
    {
        _has_deps_computed = true;
    }
    
    bool Node::is_empty_node()
    {
        return (_id==-1 && get_data<Node_type>(_NODE_TYPE) == UNCLASSIFIED_NODE);
    }
    
    ObjectList<Edge*> Node::get_entry_edges() const
    {
        return _entry_edges;
    }
    
    void Node::set_entry_edge(Edge *entry_edge)
    {
        _entry_edges.append(entry_edge);
    }
    
    ObjectList<Edge_type> Node::get_entry_edge_types()
    {
        ObjectList<Edge_type> result;
        
        for(ObjectList<Edge*>::iterator it = _entry_edges.begin();
                it != _entry_edges.end();
                ++it)
        {
                result.append((*it)->get_type());
        }
        
        return result;
    }
    
    ObjectList<std::string> Node::get_entry_edge_labels()
    {
        ObjectList<std::string> result;
        
        for(ObjectList<Edge*>::iterator it = _entry_edges.begin();
                it != _entry_edges.end();
                ++it)
        {
            result.append((*it)->get_label());
        }
        
        return result;
    }
    
    ObjectList<Node*> Node::get_parents()
    {
        ObjectList<Node*> result;
        
        for(ObjectList<Edge*>::iterator it = _entry_edges.begin();
                it != _entry_edges.end();
                ++it)
        {
            result.append((*it)->get_source());
        }
        
        return result;
    }
    
    ObjectList<Edge*> Node::get_exit_edges() const
    {
        return _exit_edges;
    }
    
    void Node::set_exit_edge(Edge *exit_edge)
    {
        _exit_edges.append(exit_edge);
    }
    
    ObjectList<Edge_type> Node::get_exit_edge_types()
    {
        ObjectList<Edge_type> result;
        
        for(ObjectList<Edge*>::iterator it = _exit_edges.begin();
                it != _exit_edges.end();
                ++it)
        {
            result.append((*it)->get_type());
        }
        
        return result;
    }
    
    ObjectList<std::string> Node::get_exit_edge_labels()
    {
        ObjectList<std::string> result;
        
        for(ObjectList<Edge*>::iterator it = _exit_edges.begin();
                it != _exit_edges.end();
                ++it)
        {
            result.append((*it)->get_label());
        }
        
        return result;
    }
    
    Edge* Node::get_exit_edge(Node* target)
    {
        Edge* result = NULL;
        int id = target->get_id();
        for(ObjectList<Edge*>::iterator it = _exit_edges.begin();
            it != _exit_edges.end();
            ++it)
        {
            if ((*it)->get_target()->get_id() == id)
            { 
                result = *it;
                break;
            }
        }
        return result;
    }
    
    ObjectList<Node*> Node::get_children()
    {
        ObjectList<Node*> result;
        
        for(ObjectList<Edge*>::iterator it = _exit_edges.begin();
                it != _exit_edges.end();
                ++it)
        {
            result.append((*it)->get_target());
        }
        
        return result;
    }

    bool Node::is_basic_node()
    {
        return ( get_type( ) != GRAPH_NODE );
    }

    bool Node::is_graph_node()
    {
        return ( get_type( ) == GRAPH_NODE );
    }
    
    bool Node::is_entry_node()
    {
        return ( get_type( ) == BASIC_ENTRY_NODE );
    }
    
    bool Node::is_exit_node()
    {
        return ( get_type( ) == BASIC_EXIT_NODE );
    }
    
    bool Node::is_graph_exit_node( Node* graph )
    {
        return ( _id == graph->get_graph_exit_node( )->get_id( ) );
    }
    
    bool Node::is_loop_node( )
    {
        return ( ( get_type( ) == GRAPH_NODE ) && ( get_graph_type( ) == LOOP ) );
    }
    
    bool Node::is_loop_stride( Node* loop )
    {
        return ( ( get_type( ) == GRAPH_NODE ) && ( get_graph_type( ) == LOOP ) 
                && ( loop->get_stride_node()->get_id( ) == _id ) );
    }
    
    bool Node::is_normal_node( )
    {
        return ( get_type( ) == BASIC_NORMAL_NODE );
    }
    
    bool Node::is_labeled_node( )
    {
        return ( get_type( ) == BASIC_LABELED_NODE );
    }
    
    bool Node::is_function_call_node( )
    {
        return ( get_type( ) == BASIC_FUNCTION_CALL_NODE );
    }
    
    bool Node::is_task_node( )
    {
        return ( is_graph_node( ) && ( get_graph_type( ) == TASK ) );
    }
    
    bool Node::is_connected()
    {
        return (!_entry_edges.empty() || !_exit_edges.empty());
    }

    bool Node::has_child(Node* n)
    {
        bool result = false;
        int id = n->_id;
        
        for (ObjectList<Edge*>::iterator it = _exit_edges.begin(); it != _exit_edges.end(); ++it)
        {
            if ((*it)->get_target()->_id == id)
            {    
                result = true;
                break;
            }
        }
        
        return result;
    }

    bool Node::has_parent(Node* n)
    {
        bool result = false;
        int id = n->_id;
        
        for (ObjectList<Edge*>::iterator it = _entry_edges.begin(); it != _entry_edges.end(); ++it)
        {
            if ((*it)->get_source()->_id == id)
            {    
                result = true;
                break;
            }
        }
        
        return result;
    }

    bool Node::operator==(const Node* &n) const
    {
        return ((_id == n->_id) && (_entry_edges == n->_entry_edges) && (_exit_edges == n->_exit_edges));
    }

    ObjectList<Node*> Node::get_inner_nodes_in_level()
    {
        if (get_data<Node_type>(_NODE_TYPE) != GRAPH_NODE)
        {
            return ObjectList<Node*>();
        }
        else
        {
            ObjectList<Node*> node_l;
            get_inner_nodes_rec(node_l);
            
            return node_l;
        }
    }

    void Node::get_inner_nodes_rec(ObjectList<Node*>& node_l)
    {
        if (!_visited)
        {
            set_visited(true);
            
            Node* actual = this;
            Node_type ntype = get_data<Node_type>(_NODE_TYPE);
            if (ntype == GRAPH_NODE)
            {
                // Get the nodes inside the graph
//                 Node* entry = get_data<Node*>(_ENTRY_NODE);
//                 entry->set_visited(true);
//                 actual = entry;
                node_l.insert(this);
            }
            else if (ntype == BASIC_EXIT_NODE)
            {
                return;
            }
            else if ((ntype == BASIC_NORMAL_NODE) || (ntype == BASIC_LABELED_NODE) || (ntype == BASIC_FUNCTION_CALL_NODE))
            {
                // Include the node into the list
                node_l.insert(this);
            }
            else if ((ntype == BASIC_GOTO_NODE) || (ntype == BASIC_BREAK_NODE) 
                    || (ntype == FLUSH_NODE) || (ntype == BARRIER_NODE) 
                    || (ntype == BASIC_PRAGMA_DIRECTIVE_NODE) || (ntype == TASKWAIT_NODE))
            {   // do nothing
            }
            else
            {
                internal_error("Unexpected kind of node '%s'", get_type_as_string().c_str());
            }
            
            ObjectList<Node*> next_nodes = actual->get_children();
            for (ObjectList<Node*>::iterator it = next_nodes.begin();
                it != next_nodes.end();
                ++it)
            {
                (*it)->get_inner_nodes_rec(node_l);
            }            
        }
        
        return;
    }

    Symbol Node::get_function_node_symbol()
    {
    
        if (get_data<Node_type>(_NODE_TYPE) != BASIC_FUNCTION_CALL_NODE)
        {
            return Symbol();
        }
        
        Nodecl::NodeclBase stmt = get_data<ObjectList<Nodecl::NodeclBase> >(_NODE_STMTS)[0];
        Symbol s;
        if (stmt.is<Nodecl::FunctionCall>())
        {
            Nodecl::FunctionCall f = stmt.as<Nodecl::FunctionCall>();
            s = f.get_called().get_symbol();
        }
        else if (stmt.is<Nodecl::VirtualFunctionCall>())
        {
            Nodecl::FunctionCall f = stmt.as<Nodecl::FunctionCall>();
            s = f.get_called().get_symbol();
        }
        
        return s;
    }
    
    Node_type Node::get_type()
    {
        if (has_key(_NODE_TYPE))
            return get_data<Node_type>(_NODE_TYPE);
        else
            return UNCLASSIFIED_NODE;
    }

    std::string Node::get_type_as_string()
    {
        std::string type = "";
        if (has_key(_NODE_TYPE))
        {
            Node_type ntype = get_data<Node_type>(_NODE_TYPE);
            switch(ntype)
            {
                case BASIC_ENTRY_NODE:              type = "BASIC_ENTRY_NODE";
                break;
                case BASIC_EXIT_NODE:               type = "BASIC_EXIT_NODE";
                break;
                case BASIC_NORMAL_NODE:             type = "BASIC_NORMAL_NODE";
                break;
                case BASIC_LABELED_NODE:            type = "BASIC_LABELED_NODE";
                break;
                case BASIC_BREAK_NODE:              type = "BASIC_BREAK_NODE";
                break;
                case BASIC_CONTINUE_NODE:           type = "BASIC_CONTINUE_NODE";
                break;
                case BASIC_GOTO_NODE:               type = "BASIC_GOTO_NODE";
                break;
                case BASIC_FUNCTION_CALL_NODE:      type = "BASIC_FUNCTION_CALL_NODE";
                break;
                case BASIC_PRAGMA_DIRECTIVE_NODE:   type = "BASIC_PRAGMA_DIRECTIVE_NODE";
                break;
                case FLUSH_NODE:                    type = "FLUSH_NODE";
                break;
                case BARRIER_NODE:                  type = "BARRIER_NODE";
                break;
                case TASKWAIT_NODE:                 type = "TASKWAIT_NODE";
                break;
                case GRAPH_NODE:                    type = "GRAPH_NODE";
                break;
                case UNCLASSIFIED_NODE:             type = "UNCLASSIFIED_NODE";
                break;
                default: internal_error("Unexpected type of node '%s'", ntype);
            };
        }
        else
        {
            internal_error("The node '%s' has no type assigned, this operation is not allowed", 0);
        }
        
        return type;
    }

    std::string Node::get_graph_type_as_string()
    {
        std::string graph_type = "";
        if (has_key(_GRAPH_TYPE))
        {
            Graph_type ntype = get_data<Graph_type>(_GRAPH_TYPE);
            switch(ntype)
            {
                case SPLIT_STMT:        graph_type = "SPLIT_STMT";
                break;
                case FUNC_CALL:         graph_type = "FUNC_CALL";
                break;
                case COND_EXPR:         graph_type = "COND_EXPR";
                break;
                case LOOP:              graph_type = "LOOP";
                break;
                case OMP_PRAGMA:        graph_type = "OMP_PRAGMA";
                break;
                case TASK:              graph_type = "TASK";
                break;
                case EXTENSIBLE_GRAPH:  graph_type = "EXTENSIBLE_GRAPH";
                break;
                default: internal_error("Unexpected type of node '%s'", ntype);
            };
        }
        else
        {
            internal_error("The node '%s' has no graph type assigned, this operation is not allowed", 0);
        }
        
        return graph_type;  
    }

    Node* Node::get_graph_entry_node()
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            return get_data<Node*>(_ENTRY_NODE);
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting the entry node to node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);
        }
    }

    void Node::set_graph_entry_node(Node* node)
    {
        if (node->get_data<Node_type>(_NODE_TYPE) != BASIC_ENTRY_NODE)
        {
            internal_error("Unexpected node type '%s' while setting the entry node to node '%s'. BASIC_ENTRY_NODE expected.",
                        get_type_as_string().c_str(), _id);
        }
        else if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            return set_data(_ENTRY_NODE, node);
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting the entry node to node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);
        }
    }

    Node* Node::get_graph_exit_node()
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            return get_data<Node*>(_EXIT_NODE);
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting the exit node to node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);
        }
    }

    void Node::set_graph_exit_node(Node* node)
    {
        if (node->get_data<Node_type>(_NODE_TYPE) != BASIC_EXIT_NODE)
        {
            internal_error("Unexpected node type '%s' while setting the exit node to node '%s'. BASIC_EXIT_NODE expected.",
                        get_type_as_string().c_str(), _id);
        }
        else if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            return set_data(_EXIT_NODE, node);
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting the exit node to node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);
        }
    }

    Node* Node::get_outer_node()
    {
        Node* outer_node = NULL;
        if (has_key(_OUTER_NODE))
        {
            outer_node = get_data<Node*>(_OUTER_NODE);
        }
        return outer_node;
    }
    
    void Node::set_outer_node(Node* node)
    {
        if (node->get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {    
            set_data(_OUTER_NODE, node);
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting the exit node to node '%s'. GRAPH_NODE expected.",
                        node->get_type_as_string().c_str(), _id);
        }
    }

    Scope Node::get_scope()
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            if (has_key(_SCOPE))
            {
                return get_data<Scope>(_SCOPE);
            }
            else
            {
                return Scope();
            }
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting the scope of the graph node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);
        } 
    }
    
    void Node::set_scope(Scope sc)
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            set_data(_SCOPE, sc);
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting the scope of the graph node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);
        } 
    }
    
    ObjectList<Nodecl::NodeclBase> Node::get_statements()
    {
        ObjectList<Nodecl::NodeclBase> stmts;
        if (has_key(_NODE_STMTS))
        {
            stmts = get_data<ObjectList<Nodecl::NodeclBase> >(_NODE_STMTS);
        }
        return stmts;
    }

    void Node::set_statements(ObjectList<Nodecl::NodeclBase> stmts)
    {
        Node_type ntype = get_data<Node_type>(_NODE_TYPE);
        if (ntype == BASIC_NORMAL_NODE || ntype == BASIC_FUNCTION_CALL_NODE || ntype == BASIC_LABELED_NODE
            || ntype == BASIC_BREAK_NODE || ntype == BASIC_CONTINUE_NODE || ntype == BASIC_GOTO_NODE)
        {
            set_data(_NODE_STMTS, stmts);
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting the statements to node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);
        }
    }

    Nodecl::NodeclBase Node::get_graph_label(Nodecl::NodeclBase n)
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            Nodecl::NodeclBase res = Nodecl::NodeclBase::null();
            if (has_key(_NODE_LABEL))
            {
                res = get_data<Nodecl::NodeclBase>(_NODE_LABEL, n);
            }
            return res;
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting the label to node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);            
        }
    }
    
    void Node::set_graph_label(Nodecl::NodeclBase n)
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            set_data(_NODE_LABEL, n);
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting the label to node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);            
        }
    }

    Graph_type Node::get_graph_type()
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            return get_data<Graph_type>(_GRAPH_TYPE);
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting graph type to node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);            
        }
    }
        
    void Node::set_graph_type(Graph_type graph_type)
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            set_data(_GRAPH_TYPE, graph_type);
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting graph type to node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);            
        }
    }

    Loop_type Node::get_loop_node_type()
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            if (get_data<Graph_type>(_GRAPH_TYPE) == LOOP)
            {
                return get_data<Loop_type>(_LOOP_TYPE);
            }
            else
            {
                internal_error("Unexpected node type '%s' while getting the loop type of the graph node '%s'. LOOP expected.",
                            get_type_as_string().c_str(), _id);
            }
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting the loop type of the graph node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);
        }
    }
        
    void Node::set_loop_node_type(Loop_type loop_type)
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            if (get_data<Graph_type>(_GRAPH_TYPE) == LOOP)
            {
                set_data(_LOOP_TYPE, loop_type);
            }
            else
            {
                internal_error("Unexpected node type '%s' while setting the loop type of the graph node '%s'. LOOP expected.",
                get_type_as_string().c_str(), _id);
            }
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting the loop type of the graph node '%s'. GRAPH_NODE expected.",
            get_type_as_string().c_str(), _id);
        }        
    }
        
        

    // ****************************************************************************** //
    // **************** Getters and setters for constants analysis ****************** //
        
    ObjectList<LatticeCellValue> Node::get_lattice_val( )
    {
        if ( get_data<Node_type>( _NODE_TYPE ) != GRAPH_NODE )
        {
            ObjectList<LatticeCellValue> result;
            if ( has_key( _LATTICE_VALS ) )
            {
                result = get_data<ObjectList<LatticeCellValue> >( _LATTICE_VALS );
            }
            return result;
        }
        else
        {
            internal_error( "Requesting Lattice Cell Values list in a GRAPH_NODE. Simple node expected.",
                            get_type_as_string( ).c_str( ), _id );
        }
    }
        
    void Node::set_lattice_val( LatticeCellValue lcv )
    {
        if ( get_data<Node_type>( _NODE_TYPE ) != GRAPH_NODE )
        {
            ObjectList<LatticeCellValue> result;
            if ( has_key( _LATTICE_VALS ) )
            {
                result = get_data<ObjectList<LatticeCellValue> >( _LATTICE_VALS );
            }
            
            result.append( lcv );
            set_data( _LATTICE_VALS, result );
        }
        else
        {
            internal_error( "Requesting Lattice Cell Values list in a GRAPH_NODE. Simple node expected.",
                            get_type_as_string( ).c_str( ), _id );
        }
    }
        
    // ************** END getters and setters for constants analysis **************** //
    // ****************************************************************************** //
        
        
        
    
    IV_map Node::get_induction_variables()
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            if (get_data<Graph_type>(_GRAPH_TYPE) == LOOP)
            {
                IV_map ivs;
                if (has_key(_INDUCTION_VARS))
                    ivs = get_data<IV_map>(_INDUCTION_VARS);
                return ivs;
            }
            else
            {
                internal_error("Unexpected node type '%s' while getting the induction variables of the node '%s'. LOOP expected.",
                            get_type_as_string().c_str(), _id);
            }
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting the induction variables of the node '%s'. GRAPH_NODE expected.",
                        get_type_as_string().c_str(), _id);
        }
    }
    
    void Node::set_induction_variable(Nodecl::NodeclBase iv, InductionVariableData iv_data)
    {
        if ( is_loop_node( ) )
        {
            IV_map ivs;
            if ( has_key(_INDUCTION_VARS) )
            {
                ivs = get_data<IV_map>( _INDUCTION_VARS );
            }
            if ( ivs.find( iv ) != ivs.end( ) )
            {
                nodecl_t iv_internal = iv.get_internal_nodecl( );
                internal_error( "Trying to insert the Induction Variable '%s' in a loop that already contains this variable", 
                                codegen_to_str(iv_internal, nodecl_retrieve_context( iv_internal ) ) );
            }
            else
            {
                ivs.insert ( std::pair<Nodecl::NodeclBase, InductionVariableData>( iv, iv_data ) );
            }
            
            set_data( _INDUCTION_VARS, ivs );
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting a induction variable in the graph node '%s'. LOOP expected.",
                           get_type_as_string().c_str(), _id);
        }
    }
    
    
    
    Symbol Node::get_label()
    {
        Node_type ntype = get_data<Node_type>(_NODE_TYPE);
        if (ntype == BASIC_GOTO_NODE || ntype == BASIC_LABELED_NODE)
        {
            return get_data<Symbol>(_NODE_LABEL);
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting the label to node '%s'. GOTO or LABELED NODES expected.",
                        get_type_as_string().c_str(), _id);            
        }  
    }
        
    void Node::set_label(Symbol s)
    {
        Node_type ntype = get_data<Node_type>(_NODE_TYPE);
        if (ntype == BASIC_GOTO_NODE || ntype == BASIC_LABELED_NODE)
        {
            set_data(_NODE_LABEL, s);
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting the label to node '%s'. GOTO or LABELED NODES expected.",
                        get_type_as_string().c_str(), _id);            
        }
    }

    Nodecl::NodeclBase Node::get_task_context()
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            Graph_type graph_type = get_data<Graph_type>(_GRAPH_TYPE);
            if (graph_type == TASK)
            {
                return get_data<Nodecl::Context>(_TASK_CONTEXT);
            }
            else
            {
                internal_error("Unexpected graph type '%s' while getting the context of the task node '%d'. " \
                            "\"task\" type expected", get_graph_type_as_string().c_str(), _id);   
            }
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting the context of the task node '%d'. \"task\" type expected.",
                        get_type_as_string().c_str(), _id);                
        }
    }

    void Node::set_task_context(Nodecl::NodeclBase c)
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            Graph_type graph_type = get_data<Graph_type>(_GRAPH_TYPE);
            if (graph_type == TASK)
            {
                return set_data(_TASK_CONTEXT, c);
            }
            else
            {
                internal_error("Unexpected graph type '%s' while setting the context of the task node '%d'. " \
                            "\"task\" type expected", get_graph_type_as_string().c_str(), _id);                  
            }
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting the label to node '%d'. GRAPH NODE expected.",
                        get_type_as_string().c_str(), _id);                
        }
    }

    Symbol Node::get_task_function()
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            Graph_type graph_type = get_data<Graph_type>(_GRAPH_TYPE);
            if (graph_type == TASK)
            {
                return get_data<Symbol>(_TASK_FUNCTION);
            }
            else
            {
                internal_error("Unexpected graph type '%s' while getting the symbol of the function embedded in the task '%s'. " \
                            "\"task\" type expected", get_graph_type_as_string().c_str(), 
                            get_data<Nodecl::NodeclBase>(_NODE_LABEL).prettyprint().c_str());
            }
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting the symbol of the function embedded in a task'. GRAPH NODE expected.",
                        get_type_as_string().c_str());
        }
    }

    void Node::set_task_function(Symbol func_sym)
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            Graph_type graph_type = get_data<Graph_type>(_GRAPH_TYPE);
            if (graph_type == TASK)
            {
                return set_data(_TASK_FUNCTION, func_sym);
            }
            else
            {
                internal_error("Unexpected graph type '%s' while setting the symbol of the function embedded in the task '%s'. " \
                            "\"task\" type expected", get_graph_type_as_string().c_str(), 
                            get_data<Nodecl::NodeclBase>(_NODE_LABEL).prettyprint().c_str());
            }
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting the symbol of the function embedded in a task. GRAPH NODE expected.",
                        get_type_as_string().c_str());
        }
    }

    Node* Node::get_stride_node()
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            Graph_type graph_type = get_data<Graph_type>(_GRAPH_TYPE);
            if (graph_type == LOOP)
            {
                return get_data<Node*>(_STRIDE_NODE);
            }
            else
            {
                internal_error("Unexpected graph type '%s' while getting the stride node of loop node '%d'. LOOP expected", 
                            get_graph_type_as_string().c_str(), _id);                  
            }
        }
        else
        {
            internal_error("Unexpected node type '%s' while getting stride node of loop graph node '%d'. GRAPH NODE expected.",
                        get_type_as_string().c_str(), _id);                
        }
    }

    void Node::set_stride_node(Node* stride)
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            Graph_type graph_type = get_data<Graph_type>(_GRAPH_TYPE);
            if (graph_type == LOOP)
            {
                set_data(_STRIDE_NODE, stride);
            }
            else
            {
                internal_error("Unexpected graph type '%s' while setting the stride node to loop node '%d'. LOOP expected", 
                            get_graph_type_as_string().c_str(), _id);                  
            }
        }
        else
        {
            internal_error("Unexpected node type '%s' while setting stride node to loop graph node '%d'. GRAPH NODE expected.",
                        get_type_as_string().c_str(), _id);                
        }
    }

    bool Node::is_stride_node()
    {
        bool res = false;
        Node* outer_node = get_outer_node();
        while (outer_node != NULL && outer_node->get_graph_type() != LOOP)
        {
            outer_node = outer_node->get_outer_node();
        }
        
        if (outer_node != NULL)
        {
            Node* stride = outer_node->get_stride_node();
            res = (stride->_id == _id);
        }
        return res;
    }
    
    bool Node::is_stride_node( Node* loop )
    {
        Node* stride = loop->get_stride_node();
        return (stride->_id == _id);
    }
    
    Node* Node::advance_over_non_statement_nodes()
    {
        ObjectList<Node*> children = get_children();
        Node_type ntype;
        Node* result = this;
        
        while ( (children.size() == 1) && (!result->has_key(_NODE_STMTS)) )
        {
            Node* c0 = children[0];
            ntype = c0->get_data<Node_type>(_NODE_TYPE);
            if (ntype == GRAPH_NODE)
            {
                result = c0->get_data<Node*>(_ENTRY_NODE);
            }
            else if (ntype == BASIC_EXIT_NODE)
            {
                if (c0->has_key(_OUTER_NODE))
                {
                    result = c0->get_data<Node*>(_OUTER_NODE);
                }
                else
                {    
                    break;
                }
            }
            else
            {
                result = c0;
            }
            children = result->get_children();
        }
    
        return result;
    }

    Node* Node::back_over_non_statement_nodes()
    {
        ObjectList<Node*> parents = get_parents();
        Node_type ntype;
        Node* result = this;
        
        while ( (parents.size() == 1) && (!result->has_key(_NODE_STMTS)) )
        {
            Node* p0 = parents[0];
            ntype = p0->get_data<Node_type>(_NODE_TYPE);
            
            if (ntype == GRAPH_NODE)
            {
                result = p0->get_data<Node*>(_EXIT_NODE);
            }
            else if (ntype == BASIC_ENTRY_NODE)
            {
                if (p0->has_key(_OUTER_NODE))
                {
                    result = p0->get_data<Node*>(_OUTER_NODE);
                }
                else
                {    
                    break;
                }
            }
            else
            {
                result = p0;
            }
            parents = result->get_parents();
        }
    
        return result;
    }

    void Node::set_graph_node_reaching_definitions()
    {
        if (get_data<Node_type>(_NODE_TYPE) == GRAPH_NODE)
        {
            set_data(_REACH_DEFS, get_data<Node*>(_EXIT_NODE)->get_data<nodecl_map>(_REACH_DEFS));
        }
        else
        {
            internal_error("Propagating reaching definitions in node '%d' with type '%s' while "
                            "here it is mandatory a Graph node.\n",
                        _id, get_type_as_string().c_str());
        }
    }
    
    Utils::ext_sym_set Node::get_live_in_vars()
    {
        Utils::ext_sym_set live_in_vars;
        
        if (has_key(_LIVE_IN))
        {
            live_in_vars = get_data<Utils::ext_sym_set>(_LIVE_IN);
        }

        return live_in_vars;
    }
    
    void Node::set_live_in(Utils::ExtendedSymbol new_live_in_var)
    {
        Utils::ext_sym_set live_in_vars;
        
        if (has_key(_LIVE_IN))
        {
            live_in_vars = get_data<Utils::ext_sym_set>(_LIVE_IN);
        }        
        live_in_vars.insert(new_live_in_var);

        set_data(_LIVE_IN, live_in_vars);
    }
    
    void Node::set_live_in(Utils::ext_sym_set new_live_in_set)
    {
        set_data(_LIVE_IN, new_live_in_set);
    }
    
    Utils::ext_sym_set Node::get_live_out_vars()
    {
        Utils::ext_sym_set live_out_vars;
        
        if (has_key(_LIVE_OUT))
        {
            live_out_vars = get_data<Utils::ext_sym_set>(_LIVE_OUT);
        }
        
        return live_out_vars;
    }
    
    void Node::set_live_out(Utils::ExtendedSymbol new_live_out_var)
    {
        Utils::ext_sym_set live_out_vars;
        
        if (has_key(_LIVE_OUT))
        {
            live_out_vars = get_data<Utils::ext_sym_set>(_LIVE_OUT);
        }
        live_out_vars.insert(new_live_out_var);
        
        set_data(_LIVE_OUT, live_out_vars);
    }
    
    void Node::set_live_out(Utils::ext_sym_set new_live_out_set)
    {
        set_data(_LIVE_OUT, new_live_out_set);
    }
    
    Utils::ext_sym_set Node::get_ue_vars()
    {
        Utils::ext_sym_set ue_vars;
        
        if (has_key(_UPPER_EXPOSED))
        {
            ue_vars = get_data<Utils::ext_sym_set>(_UPPER_EXPOSED);
        }
        
        return ue_vars;
    }
    
    void Node::set_ue_var(Utils::ExtendedSymbol new_ue_var)
    {
        Utils::ext_sym_set ue_vars;

        if (this->has_key(_UPPER_EXPOSED))
        {   
            ue_vars = get_data<Utils::ext_sym_set>(_UPPER_EXPOSED);
        }
        if (!Utils::ext_sym_set_contains_englobing_nodecl(new_ue_var, ue_vars))
        {
            ue_vars.insert(new_ue_var);
            set_data(_UPPER_EXPOSED, ue_vars);
        }
    }
    
    void Node::set_ue_var(Utils::ext_sym_set new_ue_vars)
    {
        Utils::ext_sym_set ue_vars;

        if (this->has_key(_UPPER_EXPOSED))
        {   
            ue_vars = get_data<Utils::ext_sym_set>(_UPPER_EXPOSED);
        }
        
        Utils::ext_sym_set purged_ue_vars;
        Utils::ext_sym_set::iterator it = new_ue_vars.begin();
        for (; it != new_ue_vars.end(); ++it)
        {
            if (!Utils::ext_sym_set_contains_englobing_nodecl(*it, ue_vars))
            {
                purged_ue_vars.insert(*it);
            }
        }
        if (it == new_ue_vars.end())
        {
            ue_vars.insert(purged_ue_vars);
            set_data(_UPPER_EXPOSED, ue_vars);
        }
    }
    
    void Node::unset_ue_var(Utils::ExtendedSymbol old_ue_var)
    {
        Utils::ext_sym_set ue_vars;
    
        if (has_key(_UPPER_EXPOSED))
        {   
            ue_vars = get_data<Utils::ext_sym_set>(_UPPER_EXPOSED);
            ue_vars = ue_vars.not_find(old_ue_var);
        }
        
        set_data(_UPPER_EXPOSED, ue_vars);
    }    
    
    Utils::ext_sym_set Node::get_killed_vars()
    {
        Utils::ext_sym_set killed_vars;
        
        if (has_key(_KILLED))
        {
            killed_vars = get_data<Utils::ext_sym_set>(_KILLED);
        }
        
        return killed_vars;
    }
    
    void Node::set_killed_var(Utils::ExtendedSymbol new_killed_var)
    {           
        Utils::ext_sym_set killed_vars;

        if (has_key(_KILLED))
        {
            killed_vars = get_data<Utils::ext_sym_set>(_KILLED);
        }
        
        if (!Utils::ext_sym_set_contains_englobing_nodecl(new_killed_var, killed_vars))
        {
            killed_vars.insert(new_killed_var);
            set_data(_KILLED, killed_vars);
        }
    }
    
    void Node::set_killed_var(Utils::ext_sym_set new_killed_vars)
    {
        Utils::ext_sym_set killed_vars;

        if (has_key(_KILLED))
        {
            killed_vars = get_data<Utils::ext_sym_set>(_KILLED);
        }
        
        Utils::ext_sym_set purged_killed_vars;
        Utils::ext_sym_set::iterator it = new_killed_vars.begin();
        for (; it != new_killed_vars.end(); ++it)
        {
            if (!Utils::ext_sym_set_contains_englobing_nodecl(*it, killed_vars))
            {
                purged_killed_vars.insert(*it);
            }
        }
        if (it == new_killed_vars.end())
        {
            killed_vars.insert(purged_killed_vars);
            set_data(_KILLED, killed_vars);
        }
    }
    
    void Node::unset_killed_var(Utils::ExtendedSymbol old_killed_var)
    {
        Utils::ext_sym_set killed_vars;
        
        if (has_key(_KILLED))
        {
            killed_vars = get_data<Utils::ext_sym_set>(_KILLED);
            killed_vars = killed_vars.not_find(old_killed_var);
        }
        
        set_data(_KILLED, killed_vars);        
    }

    Utils::ext_sym_set Node::get_undefined_behaviour_vars()
    {
        Utils::ext_sym_set undef_vars;
        
        if (has_key(_UNDEF))
        {
            undef_vars = get_data<Utils::ext_sym_set>(_UNDEF);
        }
        
        return undef_vars;
    }
    
    void Node::set_undefined_behaviour_var(Utils::ExtendedSymbol new_undef_var)
    {
        Utils::ext_sym_set undef_vars;
        
        if (has_key(_UNDEF))
        {
            undef_vars = get_data<Utils::ext_sym_set>(_UNDEF);
        }
        
        if (!Utils::ext_sym_set_contains_englobing_nodecl(new_undef_var, undef_vars))
        {
            undef_vars.insert(new_undef_var);
            set_data(_UNDEF, undef_vars);
        }
    }

    void Node::unset_undefined_behaviour_var(Utils::ExtendedSymbol old_undef_var)
    {
        Utils::ext_sym_set undef_vars;
        
        if (has_key(_UNDEF))
        {
            undef_vars = get_data<Utils::ext_sym_set>(_UNDEF);
            undef_vars = undef_vars.not_find(old_undef_var);
        }
        
        set_data(_UNDEF, undef_vars);   
    }

    void Node::set_undefined_behaviour_var(Utils::ext_sym_set new_undef_vars)
    {
        Utils::ext_sym_set undef_vars;
        
        if (has_key(_UNDEF))
        {
            undef_vars = get_data<Utils::ext_sym_set>(_UNDEF);
        }
        
        
        Utils::ext_sym_set purged_undef_vars;
        Utils::ext_sym_set::iterator it = new_undef_vars.begin();
        for (; it != new_undef_vars.end(); ++it)
        {
            if (!Utils::ext_sym_set_contains_englobing_nodecl(*it, undef_vars))
            {
                purged_undef_vars.insert(*it);
            }
        }
        if (it == new_undef_vars.end())
        {
            undef_vars.insert(purged_undef_vars);
            set_data(_UNDEF, undef_vars);
        }         
    }

    Utils::ext_sym_set Node::get_input_deps()
    {
        Utils::ext_sym_set input_deps;
        
        if (has_key(_IN_DEPS))
        {
            input_deps = get_data<Utils::ext_sym_set>(_IN_DEPS);
        }
        
        return input_deps;
    }    

    void Node::set_input_deps(Utils::ext_sym_set new_input_deps)
    {
        Utils::ext_sym_set input_deps;
        
        if (has_key(_IN_DEPS))
        {
            input_deps = get_data<Utils::ext_sym_set>(_IN_DEPS);
        }
        
        input_deps.insert(new_input_deps);
        set_data(_IN_DEPS, input_deps);;
    }

    Utils::ext_sym_set Node::get_output_deps()
    {
        Utils::ext_sym_set output_deps;
        
        if (has_key(_OUT_DEPS))
        {
            output_deps = get_data<Utils::ext_sym_set>(_OUT_DEPS);
        }
        
        return output_deps;
    }  
    
    void Node::set_output_deps(Utils::ext_sym_set new_output_deps)
    {
        Utils::ext_sym_set output_deps;
        
        if (has_key(_OUT_DEPS))
        {
            output_deps = get_data<Utils::ext_sym_set>(_OUT_DEPS);
        }
        
        output_deps.insert(new_output_deps);
        set_data(_OUT_DEPS, output_deps);
    }
    
    Utils::ext_sym_set Node::get_inout_deps()
    {
        Utils::ext_sym_set inout_deps;
        
        if (has_key(_INOUT_DEPS))
        {
            inout_deps = get_data<Utils::ext_sym_set>(_INOUT_DEPS);
        }
        
        return inout_deps;
    }
            
    void Node::set_inout_deps(Utils::ext_sym_set new_inout_deps)
    {
        Utils::ext_sym_set inout_deps;
        
        if (has_key(_INOUT_DEPS))
        {
            inout_deps = get_data<Utils::ext_sym_set>(_INOUT_DEPS);
        }
        
        inout_deps.insert(new_inout_deps);
        set_data(_INOUT_DEPS, inout_deps);
    }
    
    Utils::ext_sym_set Node::get_undef_deps()
    {
        Utils::ext_sym_set undef_deps;
            
        if (has_key(_UNDEF_DEPS))
        {
            undef_deps = get_data<Utils::ext_sym_set>(_UNDEF_DEPS);
        }
            
        return undef_deps;
    }
            
    void Node::set_undef_deps(Utils::ext_sym_set new_undef_deps)
    {
        Utils::ext_sym_set undef_deps;
            
        if (has_key(_UNDEF_DEPS))
        {
            undef_deps = get_data<Utils::ext_sym_set>(_UNDEF_DEPS);
        }
            
        undef_deps.insert(new_undef_deps);
        set_data(_UNDEF_DEPS, undef_deps);
    }
    
    Utils::ext_sym_set Node::get_shared_vars()
    {
        Utils::ext_sym_set shared_vars;
            
        if (has_key(_SHARED))
        {
            shared_vars = get_data<Utils::ext_sym_set>(_SHARED);
        }
            
        return shared_vars;
    }
            
    void Node::set_shared_var(Utils::ExtendedSymbol ei)
    {
        Utils::ext_sym_set shared_vars;

        if (has_key(_SHARED))
        {
            shared_vars = get_data<Utils::ext_sym_set>(_SHARED);
        }
        
        shared_vars.insert(ei);
        set_data(_SHARED, shared_vars);
    }

    void Node::set_shared_var(Utils::ext_sym_set new_shared_vars)
    {
        Utils::ext_sym_set shared_vars;

        if (has_key(_SHARED))
        {
            shared_vars = get_data<Utils::ext_sym_set>(_SHARED);
        }
        
        shared_vars.insert(new_shared_vars);
        set_data(_SHARED, shared_vars);
    }

    Utils::ext_sym_set Node::get_private_vars()
    {
        Utils::ext_sym_set private_vars;
            
        if (has_key(_PRIVATE))
        {
            private_vars = get_data<Utils::ext_sym_set>(_PRIVATE);
        }
            
        return private_vars;
    }
    
    void Node::set_private_var(Utils::ExtendedSymbol ei)
    {
        Utils::ext_sym_set private_vars;
            
        if (has_key(_PRIVATE))
        {
            private_vars = get_data<Utils::ext_sym_set>(_PRIVATE);
        }
            
        private_vars.insert(ei);
        set_data(_PRIVATE, private_vars);
    }

    void Node::set_private_var(Utils::ext_sym_set new_private_vars)
    {
        Utils::ext_sym_set private_vars;
            
        if (has_key(_PRIVATE))
        {
            private_vars = get_data<Utils::ext_sym_set>(_PRIVATE);
        }
            
        private_vars.insert(new_private_vars);
        set_data(_PRIVATE, private_vars);
    }

    Utils::ext_sym_set Node::get_firstprivate_vars()
    {
        Utils::ext_sym_set firstprivate_vars;
            
        if (has_key(_FIRSTPRIVATE))
        {
            firstprivate_vars = get_data<Utils::ext_sym_set>(_FIRSTPRIVATE);
        }
            
        return firstprivate_vars;
    }
    
    void Node::set_firstprivate_var(Utils::ExtendedSymbol ei)
    {
        Utils::ext_sym_set firstprivate_vars;
            
        if (has_key(_FIRSTPRIVATE))
        {
            firstprivate_vars = get_data<Utils::ext_sym_set>(_FIRSTPRIVATE);
        }
            
        firstprivate_vars.insert(ei);
        set_data(_FIRSTPRIVATE, firstprivate_vars);
    }
    
    void Node::set_firstprivate_var(Utils::ext_sym_set new_firstprivate_vars)
    {
        Utils::ext_sym_set firstprivate_vars;
            
        if (has_key(_FIRSTPRIVATE))
        {
            firstprivate_vars = get_data<Utils::ext_sym_set>(_FIRSTPRIVATE);
        }
            
        firstprivate_vars.insert(new_firstprivate_vars);
        set_data(_FIRSTPRIVATE, firstprivate_vars);
    }
    
    Utils::ext_sym_set Node::get_undef_sc_vars()
    {
        Utils::ext_sym_set undef_sc_vars;
            
        if (has_key(_UNDEF_SC))
        {
            undef_sc_vars = get_data<Utils::ext_sym_set>(_UNDEF_SC);
        }
            
        return undef_sc_vars;
    }
    
    void Node::set_undef_sc_var(Utils::ExtendedSymbol ei)
    {
        Utils::ext_sym_set undef_sc_vars;
            
        if (has_key(_UNDEF_SC))
        {
            undef_sc_vars = get_data<Utils::ext_sym_set>(_UNDEF_SC);
        }
            
        undef_sc_vars.insert(ei);
        set_data(_UNDEF_SC, undef_sc_vars);
    }
    
    void Node::set_undef_sc_var(Utils::ext_sym_set new_undef_sc_vars)
    {
        Utils::ext_sym_set undef_sc_vars;
            
        if (has_key(_UNDEF_SC))
        {
            undef_sc_vars = get_data<Utils::ext_sym_set>(_UNDEF_SC);
        }
            
        undef_sc_vars.insert(new_undef_sc_vars);
        set_data(_UNDEF_SC, undef_sc_vars);
    }
    
    Utils::ext_sym_set Node::get_race_vars()
    {
        Utils::ext_sym_set race_vars;
            
        if (has_key(_RACE))
        {
            race_vars = get_data<Utils::ext_sym_set>(_RACE);
        }
            
        return race_vars;
    }
    
    void Node::set_race_var(Utils::ExtendedSymbol ei)
    {
        Utils::ext_sym_set race_vars;
            
        if (has_key(_RACE))
        {
            race_vars = get_data<Utils::ext_sym_set>(_RACE);
        }
            
        race_vars.insert(ei);
        set_data(_RACE, race_vars);
    }
    
    nodecl_map Node::get_reaching_definitions()
    {
        nodecl_map reaching_defs;
        
        if (has_key(_REACH_DEFS))
        {
            reaching_defs = get_data<nodecl_map>(_REACH_DEFS);
        }
        
        return reaching_defs;
    }
    

    void Node::set_reaching_definition(Nodecl::NodeclBase var, Nodecl::NodeclBase init)
    {
        nodecl_map reaching_defs;
        if (has_key(_REACH_DEFS))
        {
            reaching_defs = get_data<nodecl_map>(_REACH_DEFS);
        }
        reaching_defs[var] = init;
        set_data(_REACH_DEFS, reaching_defs);
    }
    
    void Node::set_reaching_definition_list(nodecl_map reach_defs_l)
    {
        nodecl_map reaching_defs;
        if (has_key(_REACH_DEFS))
        {
            reaching_defs = get_data<nodecl_map>(_REACH_DEFS);
        }
        for(nodecl_map::iterator it = reach_defs_l.begin(); it != reach_defs_l.end(); ++it)
        {
            reaching_defs[it->first] = it->second;
        }
    
        set_data(_REACH_DEFS, reaching_defs);
    }    
    
    void Node::rename_reaching_defintion_var(Nodecl::NodeclBase old_var, Nodecl::NodeclBase new_var)
    {
        nodecl_map reaching_defs;
        if (has_key(_REACH_DEFS))
        {
            reaching_defs = get_data<nodecl_map>(_REACH_DEFS);
            if (reaching_defs.find(old_var) != reaching_defs.end())
            {
                Nodecl::NodeclBase init = reaching_defs[old_var];
                reaching_defs.erase(old_var);
                reaching_defs[new_var] = init;
            }
            else
            {
                std::cerr << "warning: Trying to rename reaching definition '" << old_var.prettyprint()
                        << "', which not exists in reaching definition list of node '" << _id << "'" << std::endl;
            }
        }
        set_data(_REACH_DEFS, reaching_defs);
    }
    
    nodecl_map Node::get_auxiliar_reaching_definitions()
    {
        nodecl_map aux_reaching_defs;
        
        if (has_key(_AUX_REACH_DEFS))
        {
            aux_reaching_defs = get_data<nodecl_map>(_AUX_REACH_DEFS);
        }
        
        return aux_reaching_defs;
    }
    
    void Node::set_auxiliar_reaching_definition(Nodecl::NodeclBase var, Nodecl::NodeclBase init)
    {
        nodecl_map aux_reaching_defs;
        if (has_key(_AUX_REACH_DEFS))
        {
            aux_reaching_defs = get_data<nodecl_map>(_AUX_REACH_DEFS);
        }
        aux_reaching_defs[var] = init;
        set_data(_AUX_REACH_DEFS, aux_reaching_defs);        
    }
    
    void Node::unset_reaching_definition(Nodecl::NodeclBase var)
    {
        nodecl_map reaching_defs;
        if (has_key(_REACH_DEFS))
        {
            reaching_defs = get_data<nodecl_map>(_REACH_DEFS);
            reaching_defs.erase(var);
        }
        set_data(_REACH_DEFS, reaching_defs);
    }
    
    void Node::print_use_def_chains()
    {
        if (CURRENT_CONFIGURATION->debug_options.analysis_verbose)
        {
            Utils::ext_sym_set ue_vars = get_data<Utils::ext_sym_set>(_UPPER_EXPOSED);
            std::cerr << std::endl << "      - UE VARS: ";
            for(Utils::ext_sym_set::iterator it = ue_vars.begin(); it != ue_vars.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint() << ", ";
            }
            std::cerr << std::endl;
            
            Utils::ext_sym_set killed_vars = get_data<Utils::ext_sym_set>(_KILLED);
            std::cerr << "      - KILLED VARS: ";
            for(Utils::ext_sym_set::iterator it = killed_vars.begin(); it != killed_vars.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint() << ", ";
            }
            std::cerr << std::endl;
            
            Utils::ext_sym_set undef_vars = get_data<Utils::ext_sym_set>(_UNDEF);
            std::cerr << "      - UNDEF VARS: ";
            for(Utils::ext_sym_set::iterator it = undef_vars.begin(); it != undef_vars.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint() << ", ";
            }
            std::cerr << std::endl;
        }
    }
    
    void Node::print_liveness()
    {
        if (CURRENT_CONFIGURATION->debug_options.analysis_verbose)
        {
            Utils::ext_sym_set live_in_vars = get_data<Utils::ext_sym_set>(_LIVE_IN);
            std::cerr << std::endl << "      - LIVE IN VARS: ";
            for(Utils::ext_sym_set::iterator it = live_in_vars.begin(); it != live_in_vars.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint() << ", ";
            }
            std::cerr << std::endl;
        
            Utils::ext_sym_set live_out_vars = get_data<Utils::ext_sym_set>(_LIVE_OUT);
            std::cerr << "      - LIVE OUT VARS: ";
            for(Utils::ext_sym_set::iterator it = live_out_vars.begin(); it != live_out_vars.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint() << ", ";
            }
            std::cerr << std::endl;
        }
    }
    
    void Node::print_auto_scoping()
    {
        if (CURRENT_CONFIGURATION->debug_options.analysis_verbose ||
            CURRENT_CONFIGURATION->debug_options.enable_debug_code)
        {
            Utils::ext_sym_set private_vars = get_private_vars();
            std::cerr << std::endl << "     - Private(";
            for(Utils::ext_sym_set::iterator it = private_vars.begin(); it != private_vars.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != private_vars.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;

            Utils::ext_sym_set firstprivate_vars = get_firstprivate_vars();
            std::cerr << "     - Firstprivate(";
            for(Utils::ext_sym_set::iterator it = firstprivate_vars.begin(); it != firstprivate_vars.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != firstprivate_vars.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;
                
            Utils::ext_sym_set race_vars = get_race_vars();
            std::cerr << "     - Race(";
            for(Utils::ext_sym_set::iterator it = race_vars.begin(); it != race_vars.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != race_vars.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;

            Utils::ext_sym_set shared_vars = get_shared_vars();
            std::cerr << "     - Shared(";
            for(Utils::ext_sym_set::iterator it = shared_vars.begin(); it != shared_vars.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != shared_vars.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;
            
            Utils::ext_sym_set undef_vars = get_undef_sc_vars();
            std::cerr << "     - Undef(";
            for(Utils::ext_sym_set::iterator it = undef_vars.begin(); it != undef_vars.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != undef_vars.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;
        }            
    }
    
    void Node::print_task_dependencies()
    {
        if (CURRENT_CONFIGURATION->debug_options.analysis_verbose ||
            CURRENT_CONFIGURATION->debug_options.enable_debug_code)
        {
            Utils::ext_sym_set private_deps = get_private_vars();
            std::cerr << std::endl << "     - Private(";
            for(Utils::ext_sym_set::iterator it = private_deps.begin(); it != private_deps.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != private_deps.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;

            Utils::ext_sym_set firstprivate_deps = get_firstprivate_vars();
            std::cerr << "     - Firstprivate(";
            for(Utils::ext_sym_set::iterator it = firstprivate_deps.begin(); it != firstprivate_deps.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != firstprivate_deps.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;
            
            Utils::ext_sym_set input_deps = get_input_deps();
            std::cerr << "     - Input(";
            for(Utils::ext_sym_set::iterator it = input_deps.begin(); it != input_deps.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != input_deps.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;
                
            Utils::ext_sym_set output_deps = get_output_deps();
            std::cerr << "     - Output(";
            for(Utils::ext_sym_set::iterator it = output_deps.begin(); it != output_deps.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != output_deps.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;
                
            Utils::ext_sym_set inout_deps = get_inout_deps();
            std::cerr << "     - Inout(";
            for(Utils::ext_sym_set::iterator it = inout_deps.begin(); it != inout_deps.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != inout_deps.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;
            
            Utils::ext_sym_set undef_deps = get_undef_deps();
            std::cerr << "     - Undef(";
            for(Utils::ext_sym_set::iterator it = undef_deps.begin(); it != undef_deps.end(); ++it)
            {
                std::cerr << it->get_nodecl().prettyprint();
                if (it != undef_deps.end()-1)
                    std::cerr << ", ";
            }
            std::cerr << ")" << std::endl;
        }
    }

}
}