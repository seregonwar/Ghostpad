import React from "react";
import { BrowserRouter as Router, Route, Switch } from "react-router-dom";
// Hooks
import { Provider } from "./hooks/Provider";
// Screens
import { Home } from "./screens/Home";
import { Consoles } from "./screens/Consoles";
import { Projects } from "./screens/Projects";
import { Project } from "./screens/Project";
import { CapturePreview } from "./screens/Capture";
import { Settings } from "./screens/Settings";
import { Beeper } from "./screens/Beeper";
import { SystemState } from "./screens/SystemState";
import { InputRedirect } from "./screens/InputRedirect";
import { NotFoundScreen } from "./screens/404";
// Styles
import StyInitialize from "./styles/Initialize";

const App = () => {
  return (
    <Provider>
      <StyInitialize />
      <Router>
        <Switch>
          <Route exact path="/" component={Home} />
          <Route exact path="/consoles" component={Consoles} />
          <Route exact path="/settings" component={Settings} />
          <Route exact path="/capture" component={CapturePreview} />
          <Route exact path="/projects" component={Projects} />
          <Route exact path="/projects/:id" component={Project} />
          <Route exact path="/beeper" component={Beeper} />
          <Route exact path="/systemstate" component={SystemState} />
          <Route exact path="/input" component={InputRedirect} />
          <Route component={NotFoundScreen} />
        </Switch>
      </Router>
    </Provider>
  );
};

export default App;
