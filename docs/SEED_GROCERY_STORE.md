# Seed Grocery Store

Seed Grocery Store is Seed Engine's first complete dogfood project.

It is intentionally small: the goal is to prove that a beginner can create and run a simulation game using Seed's own editor, components, presets and runtime.

## Player loop

1. Open Seed Engine.
2. Choose **New Grocery Store** in the Project Hub.
3. Open **Shop Management > Products & Stock**.
4. Check prices and restock products.
5. Press **F5** to open the shop.
6. Let customers arrive during Shop Hours.
7. Move through the Customer Queue Area and Checkout Area.
8. Confirm that a customer purchase updates inventory, wallet and metrics.
9. Stop play mode and inspect **Business Dashboard**.
10. Save the project and use **Build > Prepare Standalone Build**.

## What this proves

- Project Hub to playable project flow.
- Beginner-facing shop and product concepts.
- Shop Hours and simulation clock.
- Customer spawning and authored queue placement.
- Trigger-driven checkout.
- Payment, inventory and business metrics.
- Save and build preparation.

## Deliberately postponed

This first dogfood version does not try to be a commercial supermarket simulator. The next improvements should be driven by using it as a player:

- customer movement that feels natural
- product shelves and visual presentation
- an in-game money and stock HUD
- stronger feedback for successful and failed checkout
- save/load of runtime progress
- validation before export
- a complete standalone package

Seed Grocery Store is a test game for the engine's product direction, not a separate engine branch or a one-off demo.
