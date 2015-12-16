////////////////////////////////////////////////////////////////////////////////
/// @brief Implementation of the Traversal Execution Node
///
/// @file arangod/Aql/TraversalNode.h
///
/// DISCLAIMER
///
/// Copyright 2010-2015 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_TRAVERSAL_NODE_H
#define ARANGODB_AQL_TRAVERSAL_NODE_H 1

#include "Aql/ExecutionNode.h"
#include "Aql/Condition.h"
#include "Aql/Graphs.h"
#include "VocBase/Traverser.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                  struct SimpleTraverserExpression
// -----------------------------------------------------------------------------

    class SimpleTraverserExpression : public triagens::arango::traverser::TraverserExpression {

      public:
        triagens::aql::AstNode*        toEvaluate;
        triagens::aql::Expression*     expression;
      
        SimpleTraverserExpression (bool isEdgeAccess,
                                   triagens::aql::AstNodeType comparisonType,
                                   triagens::aql::AstNode const* varAccess,
                                   triagens::aql::AstNode* toEvaluate)
        : triagens::arango::traverser::TraverserExpression(isEdgeAccess,
                                                             comparisonType,
                                                             varAccess),
            toEvaluate(toEvaluate),
            expression(nullptr) {
        }

        SimpleTraverserExpression (triagens::aql::Ast* ast, triagens::basics::Json j)
          : triagens::arango::traverser::TraverserExpression(j.json()),
            toEvaluate(nullptr),
            expression(nullptr) {

              toEvaluate = new AstNode(ast, j.get("toEvaluate"));
        }

        ~SimpleTraverserExpression () {
          if (expression != nullptr) {
            delete expression;
          }
        }

        void toJson (triagens::basics::Json& json,
                     TRI_memory_zone_t* zone) const {
          auto op = triagens::aql::AstNode::Operators.find(comparisonType);
          
          if (op == triagens::aql::AstNode::Operators.end()) {
            THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "invalid operator for simpleTraverserExpression");
          }
          std::string const operatorStr = op->second;

          json("isEdgeAccess", triagens::basics::Json(isEdgeAccess))
              ("comparisonTypeStr", triagens::basics::Json(operatorStr))
              ("comparisonType", triagens::basics::Json(static_cast<int32_t>(comparisonType)))
              ("varAccess", varAccess->toJson(zone, true))
              ("toEvaluate", toEvaluate->toJson(zone, true))
              ("compareTo", toEvaluate->toJson(zone, true));
          
        }
    };


////////////////////////////////////////////////////////////////////////////////
/// @brief class TraversalNode
////////////////////////////////////////////////////////////////////////////////

    class TraversalNode : public ExecutionNode {

      friend class ExecutionBlock;
      friend class TraversalCollectionBlock;


////////////////////////////////////////////////////////////////////////////////
/// @brief constructor with a vocbase and a collection name
////////////////////////////////////////////////////////////////////////////////

      public:

        TraversalNode (ExecutionPlan* plan,
                       size_t id,
                       TRI_vocbase_t* vocbase,
                       AstNode const* direction,
                       AstNode const* start,
                       AstNode const* graph);

        TraversalNode (ExecutionPlan* plan,
                       triagens::basics::Json const& base);

        ~TraversalNode () {
          delete _condition;

          for (auto& it : _expressions) {
            for (auto& it2 : it.second) {
              delete it2;
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief Internal constructor to clone the node.
////////////////////////////////////////////////////////////////////////////////

     private:

        TraversalNode (ExecutionPlan* plan,
                       size_t id,
                       TRI_vocbase_t* vocbase,
                       std::vector<std::string> const& edgeColls,
                       Variable const* inVariable,
                       std::string const& vertexId,
                       TRI_edge_direction_e direction,
                       uint64_t minDepth,
                       uint64_t maxDepth);




     public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the node
////////////////////////////////////////////////////////////////////////////////

        NodeType getType () const override final {
          return TRAVERSAL;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief export to JSON
////////////////////////////////////////////////////////////////////////////////

        void toJsonHelper (triagens::basics::Json&,
                           TRI_memory_zone_t*,
                           bool) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

        ExecutionNode* clone (ExecutionPlan* plan,
                              bool withDependencies,
                              bool withProperties) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a traversal node
////////////////////////////////////////////////////////////////////////////////

        double estimateCost (size_t&) const override final;

////////////////////////////////////////////////////////////////////////////////
/// @brief Test if this node uses an in variable or constant.
////////////////////////////////////////////////////////////////////////////////

        bool usesInVariable () const{
          return _inVariable != nullptr; 
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesUsedHere () const override final {
          if (usesInVariable()) {
            return std::vector<Variable const*>{ _inVariable };
          }
          return std::vector<Variable const*>{ };
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere
////////////////////////////////////////////////////////////////////////////////

        void getVariablesUsedHere (std::unordered_set<Variable const*>& result) const override final {
          for (auto const& condVar : _conditionVariables) {
            result.emplace(condVar);
          }
          if (usesInVariable()) {
            result.emplace(_inVariable);
          }
        }


////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesSetHere
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> getVariablesSetHere () const override final {
          std::vector<Variable const*> vars { _vertexOutVariable };
          if (_edgeOutVariable != nullptr) {
            vars.emplace_back(_edgeOutVariable);
            if (_pathOutVariable != nullptr) {
              vars.emplace_back(_pathOutVariable);
            }
          }
          return vars;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* vocbase () const {
          return _vocbase;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the vertex out variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* vertexOutVariable () const {
          return _vertexOutVariable;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the vertex out variable is used
////////////////////////////////////////////////////////////////////////////////

        bool usesVertexOutVariable () const {
          return _vertexOutVariable != nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the vertex out variable
////////////////////////////////////////////////////////////////////////////////

        void setVertexOutput (Variable const* outVar) {
          _vertexOutVariable = outVar;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the edge out variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* edgeOutVariable () const {
          return _edgeOutVariable;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the edge out variable is used
////////////////////////////////////////////////////////////////////////////////

        bool usesEdgeOutVariable () const {
          return _edgeOutVariable != nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the edge out variable
////////////////////////////////////////////////////////////////////////////////

        void setEdgeOutput (Variable const* outVar) {
          _edgeOutVariable = outVar;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the path out variable is used
////////////////////////////////////////////////////////////////////////////////

        bool usesPathOutVariable () const {
          return _pathOutVariable != nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the path out variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* pathOutVariable () const {
          return _pathOutVariable;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the path out variable
////////////////////////////////////////////////////////////////////////////////

        void setPathOutput (Variable const* outVar) {
          _pathOutVariable = outVar;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the in variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* inVariable () const {
          return _inVariable;
        }

        std::string const getStartVertex () const {
          return _vertexId;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief Fill the traversal options with all values known to this node or
///        with default values.
////////////////////////////////////////////////////////////////////////////////

        void fillTraversalOptions (triagens::arango::traverser::TraverserOptions& opts) const;

        std::vector<std::string> const edgeColls () const {
          return _edgeColls;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief id of the calculation node that executes a filter for this query:
////////////////////////////////////////////////////////////////////////////////

        void setCalculationNodeId(size_t const id) {
          _CalculationNodeId = id;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief id of the calculation node that executes a filter for this query:
////////////////////////////////////////////////////////////////////////////////

        size_t getCalculationNodeId() const {
          return _CalculationNodeId;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief remember the condition to execute for early traversal abortion.
////////////////////////////////////////////////////////////////////////////////

        void setCondition(Condition* condition);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the condition for the node
////////////////////////////////////////////////////////////////////////////////

        Condition* condition () const {
          return _condition;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief which variable? -1 none, 0 Edge, 1 Vertex, 2 path
////////////////////////////////////////////////////////////////////////////////

        int checkIsOutVariable (size_t variableId) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether an access is inside the specified range
////////////////////////////////////////////////////////////////////////////////

        bool isInRange (uint64_t thisIndex, bool isEdge) const {
          if (isEdge) {
            return (thisIndex < _maxDepth);
          }
          return (thisIndex <= _maxDepth);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief check whecher min..max actualy span a range
////////////////////////////////////////////////////////////////////////////////

        bool isRangeValid() const {
          return _maxDepth >= _minDepth;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief Remember a simple comparator filter
////////////////////////////////////////////////////////////////////////////////

        void storeSimpleExpression (bool isEdgeAccess,
                                    size_t indexAccess,
                                    AstNodeType comparisonType,
                                    AstNode const* varAccess,
                                    AstNode* compareTo);

////////////////////////////////////////////////////////////////////////////////
/// @brief Returns a regerence to the simple traverser expressions
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<size_t, std::vector<triagens::arango::traverser::TraverserExpression*>> const* expressions () const {
          return &_expressions;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the database
////////////////////////////////////////////////////////////////////////////////

        TRI_vocbase_t* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex output variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _vertexOutVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex output variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _edgeOutVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex output variable
////////////////////////////////////////////////////////////////////////////////

        Variable const* _pathOutVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief input variable only used if _vertexId is unused
////////////////////////////////////////////////////////////////////////////////

        Variable const* _inVariable;

////////////////////////////////////////////////////////////////////////////////
/// @brief input vertexId only used if _inVariable is unused
////////////////////////////////////////////////////////////////////////////////

        std::string _vertexId;

////////////////////////////////////////////////////////////////////////////////
/// @brief input graphJson only used for serialisation & info
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json _graphJson;

////////////////////////////////////////////////////////////////////////////////
/// @brief The minimal depth included in the result
////////////////////////////////////////////////////////////////////////////////

        uint64_t _minDepth;

////////////////////////////////////////////////////////////////////////////////
/// @brief The maximal depth searched and included in the result
////////////////////////////////////////////////////////////////////////////////

        uint64_t _maxDepth;

////////////////////////////////////////////////////////////////////////////////
/// @brief The direction edges are followed
////////////////////////////////////////////////////////////////////////////////

        TRI_edge_direction_e _direction;

////////////////////////////////////////////////////////////////////////////////
/// @brief the edge collection cid
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> _edgeColls;

////////////////////////////////////////////////////////////////////////////////
/// @brief our graph...
////////////////////////////////////////////////////////////////////////////////

        Graph const* _graphObj;

////////////////////////////////////////////////////////////////////////////////
/// @brief id of the calculation node that executes a filter for this query:
////////////////////////////////////////////////////////////////////////////////

        size_t _CalculationNodeId;

////////////////////////////////////////////////////////////////////////////////
/// @brief early abort traversal conditions:
////////////////////////////////////////////////////////////////////////////////

        Condition* _condition;

////////////////////////////////////////////////////////////////////////////////
/// @brief variables that are inside of the condition
////////////////////////////////////////////////////////////////////////////////

        std::vector<Variable const*> _conditionVariables;

////////////////////////////////////////////////////////////////////////////////
/// @brief store a simple comparator filter
/// one vector of TraverserExpressions per matchdepth (size_t)
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<size_t, std::vector<triagens::arango::traverser::TraverserExpression*>> _expressions;
    };

  }   // namespace triagens::aql
}  // namespace triagens

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
