-- The restored login procedure references master.dbo.fn_md5, outside the backups.
-- Supply standard lowercase MD5 for its varchar input without changing that procedure.
IF OBJECT_ID(N'dbo.fn_md5') IS NULL
    EXEC(N'CREATE FUNCTION dbo.fn_md5(@input varchar(8000)) RETURNS varchar(32)
           AS BEGIN
             RETURN LOWER(CONVERT(varchar(32), HASHBYTES(''MD5'', @input), 2));
           END');
