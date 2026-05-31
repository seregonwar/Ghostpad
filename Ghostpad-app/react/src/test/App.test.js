import { render, screen } from "@testing-library/react";
import App from "../App";

test("renders the Ghostpad landing page", async () => {
  render(<App />);
  expect(await screen.findByText("GHOSTPAD")).toBeTruthy();
});
