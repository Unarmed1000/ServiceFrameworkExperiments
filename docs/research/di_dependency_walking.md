# Avoiding Constructor Walking with Dependency Injection

## The Problem

In **classic constructor injection**, if class `A` depends on `B`, and `B` depends on `C`, when you add a new dependency to `C`, you often have to:

1. Update `C`’s constructor.
2. Update `B`’s constructor to pass `C`.
3. Update `A`’s constructor to pass `B`.

This is sometimes called **"walking the new dependency up the chain"**. It becomes tedious in deep object graphs.

## Solutions / Concepts

### 1. Composition Root

- A single place (often at application startup) where you wire up all dependencies.
- Only construct top-level objects there; the DI container automatically resolves the full object graph.

```csharp
var serviceProvider = new ServiceCollection()
    .AddTransient<C>()
    .AddTransient<B>()
    .AddTransient<A>()
    .BuildServiceProvider();

var a = serviceProvider.GetRequiredService<A>(); // Everything auto-resolved
```

### 2. Property or Method Injection

- Instead of passing a dependency through every constructor, inject it via a property or method.
- Example:

```csharp
class B
{
    [Inject] // Some DI containers support this
    public C C { get; set; }
}
```

- Avoids updating the entire constructor chain but reduces compile-time safety.

### 3. Service Locator (controversial)

- The class resolves its own dependencies from a central service provider.
- Stops the “walking” problem but is less clean.

```csharp
class B
{
    private readonly C _c;
    public B(IServiceProvider sp)
    {
        _c = sp.GetRequiredService<C>();
    }
}
```

### 4. Abstract Factory

- If a dependency is needed only in certain scenarios, a factory can provide it without changing constructors everywhere.

## Summary

The main idea is: **use the DI container and composition root to avoid passing new dependencies through every constructor manually**. This is sometimes called **"avoiding constructor explosion"** or **"letting the container walk the dependency graph"**.

