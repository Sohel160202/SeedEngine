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

## Current dogfood status

The reference project now has the engine-side foundations for a complete small shop loop:

- customer movement, authored queue targets, and checkout behavior
- simulation HUD with wallet, revenue, day/time, shop status, stock, customers, and orders
- runtime save slots for progress
- project validation before export
- directly launchable standalone packages
- starter shop assets and reusable shop prefabs

The next dogfood pass should focus on player-facing completeness:

- purchase/restock feedback that is obvious without opening the editor
- a small daily objective and success state
- polished shop presentation and custom imported assets
- a simple pause/save/load menu
- one finished balance pass for prices, demand, payroll, and upgrades

Seed Grocery Store remains a reference game for proving that Seed can produce a complete simulation game, not a separate engine branch or a one-off demo.
