/*
  @copyright Steve Keen 2012
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MINSKY_H
#define MINSKY_H

#include "intrusiveMap.h"
#include "selection.h"
#include "godleyIcon.h"
#include "operation.h"
#include "evalOp.h"
#include "evalGodley.h"
#include "wire.h"
#include "plotWidget.h"
//#include "groupIcon.h"
//#include "switchIcon.h"
#include "version.h"
#include "variable.h"
#include "equations.h"
//#include "inGroupTest.h"
#include "latexMarkup.h"
#include "integral.h"
#include "variableValue.h"

#include <vector>
#include <string>
#include <set>
#include <deque>
using namespace std;

#include <ecolab.h>
#include <xml_pack_base.h>
#include <xml_unpack_base.h>
using namespace ecolab;
using namespace classdesc;

namespace minsky
{
  using namespace std;
  using classdesc::shared_ptr;

  struct RKdata; // an internal structure for holding Runge-Kutta data

  // a place to put working variables of the Minsky class that needn't
  // be serialised.
  struct MinskyExclude
  {
    EvalOpVector equations;
    vector<Integral> integrals;
    shared_ptr<RKdata> ode;
    shared_ptr<ofstream> outputDataFile;
    // A map that maps an input port to variable location that it
    // receives data from
    // map<Port*,VariableValue> inputFrom;
    bool reset_needed{true}; ///< if a new model, or loaded from disk
    bool m_edited;  

    // make copy operations just dummies, as assignment of Minsky's
    // doesn't need to change this
    MinskyExclude(): historyPtr(0) {}
    MinskyExclude(const MinskyExclude&): historyPtr(0) {}
    MinskyExclude& operator=(const MinskyExclude&) {return *this;}
  protected:
    /// save history of model for undo
    /* 
       TODO: it should be sufficient to add move semantics to pack_t,
       but for some reason copy semantics are required, requiring the
       use of shared_ptr
     */
    std::deque<classdesc::pack_t> history;
    size_t historyPtr;
  };

  /// convenience class for accessing matrix elements from a data array
  class MinskyMatrix
  {
    size_t n;
    double *data;
    CLASSDESC_ACCESS(MinskyMatrix);
  public:
    MinskyMatrix(size_t n, double* data): n(n), data(data) {}
    double& operator()(size_t i, size_t j) {return data[i*n+j];}
    double operator()(size_t i, size_t j) const {return data[i*n+j];}
  };

  enum ItemType {wire, op, var, group, godley, plot};

  class Minsky: public ValueVector, public Exclude<MinskyExclude>
  {
    CLASSDESC_ACCESS(Minsky);

    /// returns a diagnostic about an item that is infinite or
    /// NaN. Either a variable name, or and operator type.
    std::string diagnoseNonFinite() const;

    float m_zoomFactor{1};

    /// write current state of all variables to the log file
    void logVariables() const;

  protected:
    /// contents of current selection
    Selection currentSelection;

  public:

    /// reflects whether the model has been changed since last save
    bool edited() const {return m_edited;}
    void markEdited() {m_edited=true; reset_needed=true;}
    /// override automatic reset on model update
    void resetNotNeeded() {reset_needed=false;}
    /// resets the edited (dirty) flags
    void resetEdited() {m_edited=false;}

    VariableValues variableValues;

    void setGodleyIconResource(const string& s)
    {GodleyIcon::svgRenderer.setResource(s);}
    void setGroupIconResource(const string& s)
    {Group::svgRenderer.setResource(s);}

    /// @return available matching columns from other Godley tables
    /// @param currTable - this table, not included in the matching process
    //  @param ac type of column we wish matches for
    std::set<string> matchingTableColumns(GodleyTable& currTable, GodleyAssetClass::AssetClass ac);

    /// find any duplicate column, and use it as a source column for balanceDuplicateColumns
    void importDuplicateColumn(const GodleyTable& srcTable, int srcCol);
    /// makes all duplicated columns consistent with \a srcTable, \a srcCol
    void balanceDuplicateColumns(const GodleyIcon& srcTable, int srcCol);

    /// returns next integral ID to allocate to an item
    //inline int getNewId()
    //{
    //  return nextId++;
    //}
    /// resets the nextId counter to the next available
    //void resetNextId();

    EvalGodley evalGodley;

    // reset m_edited as the GodleyIcon constructor calls markEdited
    Minsky() {m_edited=false;}
    ~Minsky() {clearAllMaps();} //improve shutdown times

    GroupPtr model{new Group};

    void clearAllMaps();

    /// list the possible string values of an enum (for TCL)
    template <class E> void enumVals()
    {
      tclreturn r;
      for (size_t i=0; i < sizeof(enum_keysData<E>::keysData) / sizeof(EnumKey); ++i)
        r << enum_keysData<E>::keysData[i].name;
    }

    //    int addWire(const Wire& w) {return PortManager::addWire(w);}

    /// add a wire from item \a from, to item \a to, connecting to the
    /// toIdx port of \a to, with \a coordinates
    WirePtr addWire(const Item& from, const Item& to, unsigned toPortIdx, 
                const std::vector<float>& coords = {}); 
    void deleteWire(int id);


    /// list of available operations
    void availableOperations() {enumVals<OperationType::Type>();}
    /// list of available variable types
    void variableTypes() {enumVals<VariableType::Type>();}

    /// return list of available asset classes
    void assetClasses() {enumVals<GodleyTable::AssetClass>();}

    /// returns reference to variable defining (ie input wired) for valueId
    VariablePtr definingVar(const std::string& valueId) const {
      return dynamic_pointer_cast<VariableBase>
        (model->findAny(&Group::items, [&](ItemPtr x) {
            auto v=dynamic_cast<VariableBase*>(x.get());
            return v && v->ports.size()>1 && v->ports[1]->wires.size()>0 && v->valueId()==valueId;
          }));
    }

    /// add an operation
    int addOperation(const char* op);
    /// create a new operation that is a copy of \a id
    int copyOperation(int id);

    void deleteOperation(int op);

    /// useful for debugging wiring diagrams
    std::vector<int> unwiredOperations() const;

    int newVariable(const string& name, VariableType::Type=VariableType::flow);
    int copyVariable(int id);

    int copyGroup(int id);

    //    void deleteVariable(int id)        {variables.erase(id);}

    /**
     * Creates a new plot at (x, y)
     */
//    int newPlot(double x, double y)
//    {
//      int id = getNewId();
//      plots[id].assignPorts();  //  create plot
//      plots[id].moveTo(x, y);
//      return id;
//    }

//    void deletePlot(int id)
//    {
//      plots[id].deletePorts();
//      plots.erase(id);
//    }

//    void setGodleyCell(int id, int row, int col, const string& value)
//    {
//      godleyItems[id].setCell(row, col, value);
//    }

    /// create a group from items found in the current selection
    int createGroup();
    /// remove a group, leaving its contents in place
    void ungroup(int id);
    void saveGroupAsFile(int i, const string& fileName) const;

//    /// create a new godley icon at \a x, y
//    int addGodleyTable(float x, float y) 
//    {
//      int id=getNewId();
//      GodleyIcon* g=new GodleyIcon;
//      model->addItem(id, g);
//      g->moveTo(x,y);
//      g->table.doubleEntryCompliant=true;
//      g->update();
//      return id;
//    }
    
//    /// delete godley table icon \a id
//    void deleteGodleyTable(int id)
//    {
//      // remove all contained variables
//      if (auto g=dynamic_cast<GodleyIcon*>(model->findItem(id).get()))
//        {
//          set<VariablePtr> vars;
//          vars.insert(g->flowVars.begin(), g->flowVars.end());
//          vars.insert(g->stockVars.begin(), g->stockVars.end());
//          model->recursiveDo
//            (&Group::items,
//             [&](Items& m,Items::iterator i)
//             {
//               if (vars.count(*i))
//                 m.erase(i);
//               return false;
//             });
//        }
//      model->removeItem(id);
//    }

    void initGodleys();

//    int newNote() {
//      int id=getNewId();
//      model->addItem(id, new Item);
//      return id;
//    }
//    void deleteNote(int id) {
//      model->removeItem(id);
//    }

//    int newSwitch() {
//      int id=getNewId();
//      switchItems[id].reset(new SwitchIcon);
//      return id;
//    }
//    void deleteSwitch(int id) {
//      switchItems.erase(id);
//    }

    
    
    /// select all items in rectangle bounded by \a x0, \a y0, \a x1, \a y1 
    void select(float x0, float y0, float x1, float y1);
    /// clear selection
    void clearSelection();
    /// erase items in current selection, put copy into clipboard
    void cut();
    /// copy items in current selection into clipboard
    void copy() const;
    /// paste  clipboard as a new group. @return id of nre group
    int paste();
    void saveSelectionAsFile(const string& fileName) const;

    /// @{ override to provide clipboard handling functionality
    virtual void putClipboard(const string&) const {}
    virtual std::string getClipboard() const {return "";}
    /// @}

    /// toggle selected status of given item
    void toggleSelected(ItemType itemType, int item);

    int insertGroupFromFile(const char* file);

    /// move item from \a groupId into its parent
    void addItemToGroup(int groupId, int varId);
    void removeItemFromGroup(int groupId, int varId);

    /// add variable \a varid to group \a gid
    void addVariableToGroup(int gid, int varid, bool checkIOregions=true)
    {addItemToGroup(gid,varid); /* TODO checkIOregions */}
    /// remove variable \a varid from group \a gid
    void removeVariableFromGroup(int gid, int varid)
    {removeItemFromGroup(gid,varid);}
    /// add operation \a opid to group \a gid
    void addOperationToGroup(int gid, int opid)
    {addItemToGroup(gid,opid);}
    /// remove operation \a opid from group \a gid
    void removeOperationFromGroup(int gid, int opid)
    {removeItemFromGroup(gid,opid);}
    /// add group \a gid1 to group \a group gid
    /// @return true if successful
    bool addGroupToGroup(int gid, int gid1)
    {addItemToGroup(gid,gid1);}

    /// remove group \a gid1 from group \a group gid
    void removeGroupFromGroup(int gid, int gid1)
    {removeItemFromGroup(gid,gid1);}

    //    InGroup groupTest;
    /// current state of zoom
    float zoomFactor() const {return m_zoomFactor;}
    /// zoom by \a factor, scaling all widget's coordinates, using (\a
    /// xOrigin, \a yOrigin) as the origin of the zoom transformation
    void zoom(float xOrigin, float yOrigin,float factor);
    /// set scaling factors in all widgets, without adjusting
    /// coordinates, for use in reloading the model
    void setZoom(float);

    // runs over all ports and variables removing those not in use
    void garbageCollect();

    /// checks for presence of illegal cycles in network. Returns true
    /// if there are some
    bool cycleCheck() const;

    /// opens the log file, and writes out a header line describing
    /// names of all variables
    void openLogFile(const string&);
    /// closes log file
    void closeLogFile() {outputDataFile.reset();}

    /// construct the equations based on input data
    /// @throws ecolab::error if the data is inconsistent
    void constructEquations();
    /// evaluate the equations (stockVars.size() of them)
    void evalEquations(double result[], double t, const double vars[]);

    /// returns number of equations
    size_t numEquations() const {return 0;}//equations.size();}

    /// consistency check of the equation order. Should return
    /// true. Outputs the operation number of the invalidly ordered
    /// operation.
    bool checkEquationOrder() const;

    typedef MinskyMatrix Matrix; 
    void jacobian(Matrix& jac, double t, const double vars[]);
    
    // Runge-Kutta parameters
    double stepMin{0}; ///< minimum step size
    double stepMax{0.01}; ///< maximum step size
    int nSteps{1};     ///< number of steps per GUI update
    double epsAbs{1e-3};     ///< absolute error
    double epsRel{1e-2};     ///< relative error
    int order{4};     /// solver order: 1,2 or 4
    bool implicit{false}; /// true is implicit method used, false if explicit
    int simulationDelay{0}; /// delay in milliseconds inserted between iteration steps

    double t{0}; ///< time
    void reset(); ///<resets the variables back to their initial values
    void step();  ///< step the equations (by n steps, default 1)

    /// save to a file
    void save(const std::string& filename);
    /// load from a file
    void load(const std::string& filename);

    void exportSchema(const char* filename, int schemaLevel=1);

    /// indicate position of error on canvas
    static void displayErrorItem(float x, float y);
    /// indicate operation item has error, if visible, otherwise contining group
    void displayErrorItem(const Item& op) const;

    /// returns operation ID for a given EvalOp. -1 if a temporary
    //    int opIdOfEvalOp(const EvalOpBase&) const;

    /// return the order in which operations are applied (for debugging purposes)
    ecolab::array<int> opOrder() const;

    /// return the AEGIS assigned version number
    static const char* minskyVersion;
    string ecolabVersion() {return VERSION;}

    unsigned maxHistory{100}; ///< maximum no. of history states to save

    /// clear history
    void clearHistory() {history.clear(); historyPtr=0;}
    /// push state onto history
    void pushHistory();
    /// called periodically to ensure history up to date
    void checkPushHistory() {if (historyPtr==history.size()) pushHistory();}

    /// push current model state onto history if it differs from previous
    bool pushHistoryIfDifferent();

    /// restore model to state \a changes ago 
    void undo(int changes=1);

    /// set a Tk image to render equations to
    void renderEquationsToImage(const char* image);

    /// Converts variable(s) named by \a name into a variable of type \a type.
    /// @throw if conversion is disallowed
    void convertVarType(const std::string& name, VariableType::Type type);

    /// returns true if any variable of name \a name has a wired input
    bool inputWired(const std::string&);

  };

  /// global minsky object
  Minsky& minsky();
  /// const version to help in const correctness
  inline const Minsky& cminsky() {return minsky();}
  /// RAII set the minsky object to a different one for the current scope.
  struct LocalMinsky
  {
    LocalMinsky(Minsky& m);
    ~LocalMinsky();
  };



}

#ifdef _CLASSDESC
#pragma omit pack minsky::MinskyExclude
#pragma omit unpack minsky::MinskyExclude
#pragma omit TCL_obj minsky::MinskyExclude
#pragma omit xml_pack minsky::MinskyExclude
#pragma omit xml_unpack minsky::MinskyExclude
#pragma omit xsd_generate minsky::MinskyExclude

#pragma omit xml_pack minsky::Integral
#pragma omit xml_unpack minsky::Integral

#pragma omit pack minsky::MinskyMatrix
#pragma omit unpack minsky::MinskyMatrix
#pragma omit xml_pack minsky::MinskyMatrix
#pragma omit xml_unpack minsky::MinskyMatrix
#pragma omit xsd_generate minsky::MinskyMatrix
#endif

#include "minsky.cd"
#endif
